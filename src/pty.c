#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include "../include/pty.h"

// Orijinal terminal ayarlarını sakla, çıkışta geri yükle
static struct termios orig_termios;

static void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// Terminal'i raw moda al
static int set_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        return -1;
    }

    atexit(restore_terminal);  // program çıkışında terminal'i geri yükle

    struct termios raw = orig_termios;
    // Raw mod: karakter karakter oku, echo kapatık, özel karakterleri işleme
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |=  (CS8);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        return -1;
    }
    return 0;
}

int pty_setup_child(int slave_fd) {
    // Yeni session başlat, container PID 1 olacak
    if (setsid() == -1) {
        perror("pty_setup_child: setsid");
        return -1;
    }

    // slave_fd'yi controlling terminal yap
    if (ioctl(slave_fd, TIOCSCTTY, 0) == -1) {
        perror("pty_setup_child: TIOCSCTTY");
        return -1;
    }

    // stdin, stdout, stderr'i slave PTY'ye yönlendir
    if (dup2(slave_fd, STDIN_FILENO)  == -1 ||
        dup2(slave_fd, STDOUT_FILENO) == -1 ||
        dup2(slave_fd, STDERR_FILENO) == -1) {
        perror("pty_setup_child: dup2");
        return -1;
    }

    // Orijinal slave_fd'yi kapat, artık gerek yok
    if (slave_fd > STDERR_FILENO)
        close(slave_fd);

    return 0;
}

int pty_run(int master_fd) {
    // Terminal'i raw moda al
    if (set_raw_mode() == -1)
        return -1;

    // Host terminal boyutunu container'a ilet
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0)
        ioctl(master_fd, TIOCSWINSZ, &ws);

    char buf[4096];
    fd_set fds;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);  // kullanıcı inputu
        FD_SET(master_fd, &fds);     // container outputu

        if (select(master_fd + 1, &fds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) continue;  // sinyal geldi, tekrar dene
            perror("pty_run: select");
            break;
        }

        // Kullanıcı bir şey yazdı → container'a gönder
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) break;
            if (write(master_fd, buf, n) != n) break;
        }

        // Container bir şey yazdı → terminale bas
        if (FD_ISSET(master_fd, &fds)) {
            ssize_t n = read(master_fd, buf, sizeof(buf));
            if (n <= 0) break;  // container kapandı
            if (write(STDOUT_FILENO, buf, n) != n) break;
        }
    }

    return 0;
}
