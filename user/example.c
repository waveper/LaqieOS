extern void prints(const char *str, int len);

static const char title[] = "LaqieOS user example\r\n";
static const char line1[] = "This program was loaded from the FAT image.\r\n";
static const char line2[] = "It uses the user-mode prints syscall.\r\n";

void main(void) {
  prints(title, sizeof(title) - 1);
  prints(line1, sizeof(line1) - 1);
  prints(line2, sizeof(line2) - 1);
}
