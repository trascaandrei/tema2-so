#include "util/so_stdio.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <stdio.h>

#define BUFF_SIZE 4096

struct _so_file {
	int fd;
	long pos;
	int errors;
	int eof;
	unsigned char buff[BUFF_SIZE];
	int buf_pos;
	int last_op;
};

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *file = malloc(sizeof(SO_FILE));
	
	if (file == NULL) {
		return NULL;
	}

	file->pos = 0;
	file->errors = 0;
	file->eof = 0;
	file->buf_pos = 0;
	file->last_op = -1;
	memset(file->buff, 0, BUFF_SIZE);

	if (strcmp(mode, "r") == 0) {
		file->fd = open(pathname, O_RDONLY);
	} else if (strcmp(mode, "r+") == 0) {
		file->fd = open(pathname, O_RDWR);
	} else if (strcmp(mode, "w") == 0) {
		file->fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	} else if (strcmp(mode, "w+") == 0) {
		file->fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
	} else if (strcmp(mode, "a") == 0) {
		file->fd = open(pathname, O_WRONLY | O_APPEND | O_CREAT, 0644);
	} else if (strcmp(mode, "a+") == 0) {
		file->fd = open(pathname, O_RDWR | O_APPEND | O_CREAT, 0644);
	} else {
		free(file);
		return NULL;
	}

	if (file->fd < 0) {
		free(file);
		return NULL;
	}

	return file;
}

int so_fclose(SO_FILE *stream)
{
	if (stream == NULL) {
		return 0;
	}

	so_fflush(stream);

	if (close(stream->fd) < 0) {
		return SO_EOF;
	}
	free(stream);
	return 0;
}

int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_feof(SO_FILE *stream)
{
	return stream->eof;
}

int so_ferror(SO_FILE *stream)
{
	return stream->errors;
}

int fill_buffer(SO_FILE *stream)
{
	if (read(stream->fd, stream->buff, BUFF_SIZE) == 0)
		return EOF;

	stream->buf_pos = 0;

	return 0;
}

int so_fgetc(SO_FILE *stream)
{	
	if (stream->buf_pos == BUFF_SIZE || stream->last_op == -1)
		if (fill_buffer(stream) == EOF)
			return EOF;

	stream->last_op = 0;

	stream->pos++;
	
	return stream->buff[stream->buf_pos++];
}

int so_fputc(int c, SO_FILE *stream)
{
	if (stream->buf_pos == BUFF_SIZE && stream->last_op != -1)
		so_fflush(stream);

	if (stream->last_op == -1)
		stream->buf_pos = 0;

	stream->last_op = 1;

	stream->pos++;
	
	stream->buff[stream->buf_pos++] = c;

	return c;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	size_t i = 0;
	unsigned char *p = ptr;

	for (i = 0; i < size * nmemb; i++) {
		*p = so_fgetc(stream);

		if (*p == EOF) {
			if (i == 0)
				return 0;
			else
				break;
		}

		p++;
	}

	return i / size;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	unsigned char *p = (unsigned char *)ptr;
	size_t i;

	for (i = 0; i < size * nmemb; i++) {
		so_fputc(*p, stream);
		p++;
	}

	return i / size;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	if (stream->last_op == 1)
		so_fflush(stream);

	stream->pos = lseek(stream->fd, offset, whence);

	if (stream->pos == -1) {
		return -1;
	}

	return 0;
}

long so_ftell(SO_FILE *stream)
{
	if (stream->pos == -1)
		return -1;

	return stream->pos;
}

int so_fflush(SO_FILE *stream)
{
	write(stream->fd, stream->buff, stream->buf_pos);
	memset(stream->buff, 0, BUFF_SIZE);
	stream->buf_pos = 0;

	return 0;
}

SO_FILE *so_popen(const char *command, const char *type)
{

}

int so_pclose(SO_FILE *stream)
{

}