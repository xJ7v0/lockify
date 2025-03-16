#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <X11/extensions/scrnsaver.h>

#define BUF_LEN (10 * (sizeof(struct inotify_event) + 1024))  // Buffer size for inotify events
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
	sleep(3);
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
		while (ptr < mapped_config + config_file_size && !isdigit(*ptr))
			ptr++;

		// If we find a number, convert it using atoi()
		if (ptr < mapped_config + config_file_size && (isdigit(*ptr)))
		{
			int num = atoi(ptr);  // Convert string to integer
			if (munmap(mapped_config, config_file_size) == -1) {
				perror("munmap");
				close(fd);
			}
			return num;
		}
	}

	// Just use a default 2 minutes
	return 120;
}

int main (void)
{
	Display* display;
	int idle;
	char config_dir[PATH_MAX];
	char config_file_path[PATH_MAX];

	char buf[BUF_LEN] __attribute__((aligned(8)));
	ssize_t len;
	struct inotify_event *event;

	strcat(config_dir, get_xdg_config_home());
	strcat(config_dir, "/lockify");
	create_directory(config_dir);

	strncpy(config_file_path, config_dir, sizeof(config_file_path));
	strcat(config_file_path, "/config");

	int wait = get_config(config_file_path);

	int fd = inotify_init();
	if (fd == -1)
	{
		perror("inotify_init");
		return -1;
	}

	int flags = fcntl(fd, F_GETFL, 0);

	if (flags == -1)
	{
		perror("fcntl(F_GETFL)");
		close(fd);
        	return -1;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		perror("fcntl(F_SETFL)");
		close(fd);
		return 1;
 	}

	int wd = inotify_add_watch(fd, config_file_path, IN_MODIFY);
	if (wd == -1)
	{
		perror("inotify_add_watch");
		close(fd);
		return -1;
	}

	if (!(display = XOpenDisplay (0)))
	{
		fprintf (stderr, "Couldn't connect to %s\n", XDisplayName (0));
		return -1;
	}

	(void) XSync (display, 0);

	while(1)
	{
		if (wait)
		{
			while ((idle = seconds_idle(display)) < wait)
			{
				len = read(fd, buf, BUF_LEN);
				if (len == -1 && errno != EINTR && errno != EAGAIN)
				{
					perror("read");
					break;
				}

				for (char *ptr = buf; ptr < buf + len;)
				{
					event = (struct inotify_event *) ptr;

					if (event->mask & IN_MODIFY)
						wait = get_config(config_file_path);

					ptr += sizeof(struct inotify_event) + event->len;
				}

				usleep(wait - idle);
			}

			int system_ret = system("physlock");

			if (system_ret == 127)
			{
				write(2, "No physlock found\n", 17);
				break;
			}

		} else {
			write(2, "No config file.\n", 16);
			break;
		}
	}

	inotify_rm_watch(fd, wd);
	close(fd);

	return -1;
}
