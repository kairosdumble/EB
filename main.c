/*
트리거 핀 : 초음파 발사 시작 신호 - 트리거 핀에 10마이크로초 이상 HIGH 신호를 주면 센서는 40kHz 초음파 신호 8개 펄스를 공중으로 발사함
에코 핀: 초음파 반사 수신후 HIGH 유지 시간 - 초음파가 반사되어 센서로 돌아오면 ECHO핀이 HIGH로 바뀌고 초음파의 왕복시간동안 유지
거리(cm) =ECHO 핀의 HIGH시간 /58
*/
#define _GNU_SOURCE
#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define BTN1 17
#define BTN2 27
#define BTN3 22
#define TRIG_PIN 23
#define ECHO_PIN 24
#define CHIPNAME "gpiochip0"
#define SERVER_URL "https://rfid-46414-default-rtdb.asia-southeast1.firebasedatabase.app/buttons.json"

// LCD에 출력할 문자열 받아오기
void fetch_question(char *buffer, size_t size) {
    FILE *fp = popen("curl -s https://rfid-46414-default-rtdb.asia-southeast1.firebasedatabase.app/question.json", "r");
    if (!fp) {
        perror("popen error");
        snprintf(buffer, size, "fetch fail");
        return;
    }

    fgets(buffer, size, fp);
    pclose(fp);

    // 큰따옴표 제거
    char *start = strchr(buffer, '\"');
    if (start) {
        char *end = strrchr(start + 1, '\"');
        if (end) *end = '\0';
        memmove(buffer, start + 1, strlen(start + 1) + 1);
    }
}
// Python으로 LCD 출력 실행
void display_on_lcd(const char *text) {
    char command[256];
    snprintf(command, sizeof(command), "python3 display_lcd.py \"%s\"", text);
    system(command);
}

// 타임스탬프 생성
void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// 서버 전송
void send_to_server(int button_number) {
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));

    char command[512];
    snprintf(command, sizeof(command),
             "curl -X POST -H \"Content-Type: application/json\" "
             "-d '{\"timestamp\":\"%s\", \"switch\":%d}' %s",
             timestamp, button_number, SERVER_URL);
    system(command);
}

// 마이크로초 계산
long get_microseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line *trig_line, *echo_line;
    struct gpiod_line *btn1, *btn2, *btn3;
    long start, end, duration;
    float distance;
    char msg[128];

    chip = gpiod_chip_open_by_name(CHIPNAME);
    if (!chip) {
        perror("gpiod_chip_open_by_name");
        return 1;
    }

    trig_line = gpiod_chip_get_line(chip, TRIG_PIN);
    echo_line = gpiod_chip_get_line(chip, ECHO_PIN);
    btn1 = gpiod_chip_get_line(chip, BTN1);
    btn2 = gpiod_chip_get_line(chip, BTN2);
    btn3 = gpiod_chip_get_line(chip, BTN3);

    gpiod_line_request_input_flags(btn1, "btn1", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
    gpiod_line_request_input_flags(btn2, "btn2", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
    gpiod_line_request_input_flags(btn3, "btn3", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
    gpiod_line_request_output(trig_line, "trig", 0);
    gpiod_line_request_input(echo_line, "echo");

    fetch_question(msg, sizeof(msg));
    display_on_lcd(msg); 
    while (1) {
        gpiod_line_set_value(trig_line, 0);
        usleep(2);
        gpiod_line_set_value(trig_line, 1);
        usleep(10);
        gpiod_line_set_value(trig_line, 0);

        while (gpiod_line_get_value(echo_line) == 0);
        start = get_microseconds();
        while (gpiod_line_get_value(echo_line) == 1);
        end = get_microseconds();

        distance = (end - start) * 0.0343 / 2;
        printf("Distance: %.2f cm\n", distance);

        if (distance < 10) {
            int pressed = 0;

            if (gpiod_line_get_value(btn1) == 0) pressed = 1;
            else if (gpiod_line_get_value(btn2) == 0) pressed = 2;
            else if (gpiod_line_get_value(btn3) == 0) pressed = 3;

            if (pressed) {
                printf("%d번 스위치가 눌렸습니다.\n", pressed);
                send_to_server(pressed);
                fetch_question(msg, sizeof(msg));
                sleep(1); // 2초 대기
            }
        }
        sleep(1);
    }

    gpiod_chip_close(chip);
    return 0;
}
