// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>

HANDLE handle;
uint8_t *buf;
uint64_t s[2];

static void get_error(void)
{
	DWORD err;
	char err_buf[256];

	err = GetLastError();
	printf("err in hex: %#010x\n", err);
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err_buf, 256, NULL);
	printf("%s\n", err_buf);
}

uint64_t xorshift128plus(void)
{
	uint64_t x = s[0];
	uint64_t const y = s[1];

	s[0] = y;
	x ^= x << 23;
	s[1] = x ^ y ^ (x >> 17) ^ (y >> 26);
	return s[1] + y;
}

/*
 * basic random buffer initialization - PRNG
 */
static void rand_buf(size_t size, uint8_t *data)
{
	uint64_t *p = (uint64_t *) data;
	uint64_t nsize = size / sizeof(uint64_t);

	for (uint64_t i = 0; i < nsize; i++)
		p[i] = xorshift128plus();
}

void init_drive(char *drive_name, int block_size)
{
	handle = CreateFile(drive_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		get_error();
		exit(-2);
	}
}

void tape_erase(int block_size)
{
	TAPE_SET_MEDIA_PARAMETERS params;
	DWORD res;

	memset(&params, 0, sizeof(params));
	params.BlockSize = (DWORD)block_size;
	res = SetTapeParameters(handle, SET_TAPE_MEDIA_INFORMATION, &params);
	if (res != NO_ERROR) {
		printf("Failed to set block size.\n");
		get_error();
		CloseHandle(handle);
		exit(-3);
	}
	res = EraseTape(handle, TAPE_ERASE_SHORT, FALSE);
	if (res != NO_ERROR) {
		printf("Failed to erase tape.\n");
		get_error();
		CloseHandle(handle);
		exit(-4);
	}
	printf("Tape was erased\n");
}

int tape_read(uint8_t *buf, unsigned long size)
{
	DWORD read = 0;
	BOOL res;
	DWORD err;

	res = ReadFile(handle, buf, size, &read, 0);
	if (!res) {
		err = GetLastError();
		if (err == ERROR_NO_DATA_DETECTED)
			return 0;
		if (err == ERROR_FILEMARK_DETECTED)
			return 0;
		printf("Failed to read data from tape.\n");
		get_error();
		return -1;
	}
	return read;
}

int tape_write(uint8_t *buf, unsigned long size)
{
	DWORD written = 0;
	BOOL res;
	DWORD err;

	res = WriteFile(handle, buf, size, &written, 0);
	if (!res) {
		err = GetLastError();
		if (err == ERROR_END_OF_MEDIA)
			printf("HARDWARE EOM received, buffer size %d, written size %d", size, written);
		else {
			printf("Failed to write data to tape.\n");
			get_error();
			return -1;
		}
	}
	return written;
}

void tape_rewind(int block_size)
{
	TAPE_SET_MEDIA_PARAMETERS params;
	DWORD res;

	memset(&params, 0, sizeof(params));
	params.BlockSize = (DWORD)block_size;
	res = SetTapeParameters(handle, SET_TAPE_MEDIA_INFORMATION, &params);
	if (res != NO_ERROR) {
		printf("Failed to set block size.\n");
		get_error();
		CloseHandle(handle);
		exit(-3);
	}
	res = SetTapePosition(handle, TAPE_REWIND, 0, 0, 0, FALSE);
	if (res != NO_ERROR) {
		printf("Tape positioning error.\n");
		get_error();
		CloseHandle(handle);
		exit(-5);
	}
}

int main(int argc, char **argv)
{
	if (argc != 5) {
		printf("Error: specify drive name, block-size, command and bytes to write/read. Also make sure that medium is loaded to drive.\n");
		printf("Example: tapetest.exe \"\\\\.\\Tape0\" 262144 write 107374182400\n");
		printf("Example: tapetest.exe \"\\\\.\\Tape0\" 262144 read 107374182400\n");
		return -1;
	}

	srand((unsigned int) time(NULL));
	s[1] = rand();
	s[0] = rand();

	char *drive_name = argv[1];
	int block_size = atoi(argv[2]);
	char *cmd = argv[3];
	uint64_t data_size = _atoi64(argv[4]);

	printf("Input parameters: %s %d %s %I64d\n", drive_name, block_size, cmd, data_size);
	init_drive(drive_name, block_size);

	buf = (uint8_t *) malloc(block_size);
	int buf_size = block_size;
	uint64_t total_read = 0;
	uint64_t total_written = 0;
	int read = 0;
	int written = 0;

	if (strcmp(cmd, "read") == 0) {
		tape_rewind(block_size);
		printf("Reading from tape...\n");
		while (total_read < data_size) {
			read = tape_read(buf, buf_size);
			if (read <= 0)
				break;
			total_read += read;
		}
		printf("Total bytes read from tape: %I64d\n", total_read);
	}

	if (strcmp(cmd, "write") == 0) {
		tape_erase(block_size);
		tape_rewind(block_size);
		printf("Writing to tape...\n");
		while (total_written < data_size) {
			rand_buf(buf_size, buf);
			written = tape_write(buf, buf_size);
			if (written <= 0)
				break;
			total_written += written;
		}
		printf("Total bytes written to tape: %I64d\n", total_written);
	}

	CloseHandle(handle);
	return 0;
}
