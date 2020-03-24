#include "util/so_stdio.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <paths.h>

#include <stdio.h>

#define BUFF_SIZE 4096

/* fd       -> file descriptor
 * pos      -> pozitia in fisier
 * buff     -> bufferul de citire/scriere
 * buf_size -> dimensiunea bufferului la momentul curent
 * buf_pos  -> pozitia curenta in buffer
 * errors   -> rezultatul intors de ferror
 * eof      -> rezultatul intors de feof
 * last_op  -> ultima operatie efectuata. Este egal cu:
 *				- -1, nu s-a efectuat nicio operatie
 *				-  0, ultima operatie a fost de citire
 *				-  1, ultima operatie a fost de scriere
 * mode     -> modul in care a fost deschis fisierul. Este egal cu:
 *				- -1, readonly
 *				-  0, writeonly sau read+write
 *				-  1, append
 */
struct _so_file {
	int fd;
	long pos;
	unsigned char buff[BUFF_SIZE];
	int buf_size;
	int buf_pos;
	int errors;
	int eof;
	int last_op;
	int mode;
};

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *file = malloc(sizeof(SO_FILE));

	if (file == NULL)
		return NULL;

	file->pos = 0;
	file->errors = 0;
	file->buf_size = 0;
	file->eof = 0;
	file->buf_pos = 0;
	file->last_op = -1;
	file->mode = -1;
	memset(file->buff, 0, BUFF_SIZE);

	if (strcmp(pathname, "") == 0)
		return file;


	if (strcmp(mode, "r") == 0) {
		file->fd = open(pathname, O_RDONLY);
		file->mode = -1;
	} else if (strcmp(mode, "r+") == 0) {
		file->fd = open(pathname, O_RDWR);
		file->mode = 0;
	} else if (strcmp(mode, "w") == 0) {
		file->fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		file->mode = 0;
	} else if (strcmp(mode, "w+") == 0) {
		file->fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
		file->mode = 0;
	} else if (strcmp(mode, "a") == 0) {
		file->fd = open(pathname, O_WRONLY | O_APPEND | O_CREAT, 0644);
		file->mode = 1;
	} else if (strcmp(mode, "a+") == 0) {
		file->fd = open(pathname, O_RDWR | O_APPEND | O_CREAT, 0644);
		file->mode = 1;
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
	if (stream == NULL)
		return SO_EOF;


	// golim bufferul
	int ret = so_fflush(stream);

	if (close(stream->fd) != 0)
		ret = SO_EOF;

	free(stream);

	return ret;
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

// functie pentru umplerea bufferului din stream
int fill_buffer(SO_FILE *stream)
{
	stream->buf_size = read(stream->fd, stream->buff, BUFF_SIZE);
	if (stream->buf_size == 0) {
		stream->eof = 1;
		return SO_EOF;
	}

	stream->buf_pos = 0;

	return 0;
}

int so_fgetc(SO_FILE *stream)
{
	// daca am citit tot bufferul sau daca ultima operatie nu a fost
	// de citire, atunci reumplem bufferul
	if ((stream->buf_pos == stream->buf_size && stream->last_op == 0)
		|| stream->last_op != 0)
		if (fill_buffer(stream) == SO_EOF)
			return SO_EOF;

	stream->last_op = 0;

	stream->pos++;

	return stream->buff[stream->buf_pos++];
}

int so_fputc(int c, SO_FILE *stream)
{
	// daca bufferul este plin, atunci il golim
	if (stream->buf_pos == BUFF_SIZE && stream->last_op == 1)
		if (so_fflush(stream) == -1)
			return SO_EOF;

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
		int res = so_fgetc(stream);

		if (res == SO_EOF) {
			if (i == 0)
				return 0;
			else
				break;
		} else
			*p = res;

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
	// daca ultima operatie a fost de scriere, atunci golim bufferul
	if (stream->last_op == 1)
		so_fflush(stream);

	if (stream->last_op == 0)
		stream->buf_pos = BUFF_SIZE;

	stream->pos = lseek(stream->fd, offset, whence);

	if (stream->pos == -1)
		return -1;

	return 0;
}

long so_ftell(SO_FILE *stream)
{
	return stream->pos;
}

int so_fflush(SO_FILE *stream)
{
	// daca nu am executat nicio operatie de citire sau scriere,
	// atunci nu avem ce sa golim din buffer
	if (stream->mode == -1)
		return 0;

	// daca ultima operatie a fost de scriere, atunci mutam cursorul
	// la finalul fisierului
	if (stream->mode == 1)
		stream->pos = lseek(stream->fd, 0, SEEK_END);

	int ret = write(stream->fd, stream->buff, stream->buf_pos);

	memset(stream->buff, 0, BUFF_SIZE);
	stream->buf_pos = 0;

	if (ret == -1) {
		stream->errors = 1;
		return SO_EOF;
	}

	return 0;
}

static struct pid {
	SO_FILE *fp;
	pid_t pid;
} *cur;

SO_FILE *so_popen(const char *command, const char *type)
{
	SO_FILE *stream;
	int pdes[2];
	pid_t pid;
	char *argp[] = {"sh", "-c", NULL, NULL};

	cur = malloc(sizeof(struct pid));

	if (cur == NULL)
		return NULL;
	if (pipe(pdes) < 0) {
		free(cur);
		return NULL;
	}

	pid = fork();

	if (pid < 0) {
		free(cur);
		return NULL;
	}

	if (pid == 0) {
		// in functie de modul folosit, duplicam un fd si
		// il inchidem pe celalalt.
		if (*type == 'r') {
			close(pdes[0]);
			if (pdes[1] != STDOUT_FILENO) {
				dup2(pdes[1], STDOUT_FILENO);
				close(pdes[1]);
			}
		} else {
			close(pdes[1]);
			if (pdes[0] != STDIN_FILENO) {
				dup2(pdes[0], STDIN_FILENO);
				close(pdes[0]);
			}
		}
		argp[2] = (char *)command;
		execv(_PATH_BSHELL, argp);
	}

	if (*type == 'r') {
		stream = so_fopen("", type);
		stream->fd = pdes[0];
		close(pdes[1]);
	} else {
		stream = so_fopen("", type);
		stream->fd = pdes[1];
		close(pdes[0]);
	}

	// memoram datele despre proces pentru a putea elibera resursele
	// la final
	cur->fp = stream;
	cur->pid =  pid;
	return stream;
}

int so_pclose(SO_FILE *stream)
{
	int pstat;
	int count = 0;
	pid_t pid;

	if (cur == NULL)
		return -1;
	so_fclose(stream);

	do {
		pid = waitpid(cur->pid, &pstat, 0);
		count++;
		if (count > 999999)
			break;
	} while (pid == -1);

	free(cur);

	if (pid == -1)
		return pid;
	else
		return pstat;
}
