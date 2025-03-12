#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <X11/extensions/scrnsaver.h>

#define HOME_ENV "HOME"

static XScreenSaverInfo* xss_info = 0;

static inline int create_directory(const char *path) {
	struct stat st = {0};
	if (stat(path, &st) == -1)
	{
		if (mkdir(path, 0777) == -1)
		{
			perror("mkdir failed");
			return -1;
		}
	}

	return 0;
}

static inline char* get_xdg_config_home()
{
	const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
	static char result[PATH_MAX];

	if (xdg_config_home != NULL && xdg_config_home[0] != '\0')
	{
		size_t len = strlen(xdg_config_home);
		strncpy(result, xdg_config_home, len);
		result[sizeof(result) - 1] = '\0';
		return result;
	} else {
		const char* home_dir = getenv(HOME_ENV);
		if (home_dir != NULL)
		{
			char* default_config_home = malloc(strlen(home_dir) + strlen("/.config") + 1);
			if (default_config_home != NULL)
			{
				strcpy(default_config_home, home_dir);
				strcat(default_config_home, "/.config");
				return default_config_home;
			}
		}
	}
	return NULL;
}

static inline int seconds_idle(Display *display)
{
	if (!xss_info )
		xss_info = XScreenSaverAllocInfo();
	XScreenSaverQueryInfo(display, DefaultRootWindow(display), xss_info);
	return xss_info->idle / 1000;
}

static inline int get_config(char *path)
{
	int fd = open(path, O_RDONLY);

	if (fd == -1)
	{
		perror("Error opening file");
		return -1;
	}

	struct stat file_stat;
	if (fstat(fd, &file_stat) == -1)
	{
		perror("Error getting file stats");
		close(fd);
		return -1;
	}

	size_t config_file_size = file_stat.st_size;
	char *mapped_config = mmap(NULL, config_file_size, PROT_READ, MAP_PRIVATE, fd, 0);

	if (mapped_config == MAP_FAILED) {
		perror("Error memory-mapping the file");
		close(fd);
		return -1;
	}

	char *ptr = mapped_config;

	while (ptr < mapped_config + config_file_size)
	{
		// Skip over any non-numeric characters
		while (ptr < mapped_config + config_file_size && !isdigit(*ptr) && *ptr != '-' && *ptr != '+')
			ptr++;

		// If we find a number, convert it using atoi()
		if (ptr < mapped_config + config_file_size && (isdigit(*ptr) || *ptr == '-' || *ptr == '+'))
		{
			int num = atoi(ptr);  // Convert string to integer
			return num;
		}
	}

	return 0;
}

int main (void)
{
	Display* display;
	int idle;
	char config_dir[PATH_MAX];
	char config_file_path[PATH_MAX];

	strcat(config_dir, get_xdg_config_home());
	strcat(config_dir, "/lockify");
	create_directory(config_dir);

	strncpy(config_file_path, config_dir, sizeof(config_file_path));
	strcat(config_file_path, "/config");

	int wait = get_config(config_file_path);

	if (!(display = XOpenDisplay (0))) {
		fprintf (stderr, "Couldn't connect to %s\n", XDisplayName (0));
		return 1;
	}

	(void) XSync (display, 0);

	loop:
	if (wait)
	{
		while ((idle = seconds_idle(display)) < wait)
			usleep(wait - idle);

		if (system("physlock") == 0)
			goto loop;
		else
		{
			write(2, "No physlock found\n", 17);
			return -1;
		}

	} else {
		write(2, "No config file.\n", 16);
		return -1;
	}
}
