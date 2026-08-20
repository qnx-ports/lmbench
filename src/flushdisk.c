#if defined(linux) || defined(__QNX__)
/*
 * flushdisk() - block cache clearing
 */

#include	<stdio.h>
#include	<sys/types.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<stdlib.h>

#ifdef linux
#include	<sys/ioctl.h>
#include	<sys/mount.h>
#endif

#ifdef __QNX__
#include	<string.h>
#include	<errno.h>
#include	<devctl.h>
#include	<sys/dcmd_blk.h>
#endif

int
flushdisk(int fd)
{
#ifdef linux
	int	ret = ioctl(fd, BLKFLSBUF, 0);
	usleep(100000);
	return (ret);
#elif defined(__QNX__)
	pgcache_ctl_t ctl;
	int ret;

	memset(&ctl, 0, sizeof(ctl));
	ctl.op = DCMD_FSYS_PGCACHE_CTL_OP_DISCARD;

	ret = devctl(fd, DCMD_FSYS_PGCACHE_CTL, &ctl, sizeof(ctl), NULL);
	usleep(100000);

	if (ret != EOK) {
		errno = ret;
		return -1;
	}

	return 0;
#endif
}

#endif

#ifdef	MAIN
int
main(int ac, char **av)
{
#if defined(linux) || defined(__QNX__)
	int	fd;
	int	i;

	for (i = 1; i < ac; ++i) {
		fd = open(av[i], 0);
		if (flushdisk(fd)) {
			exit(1);
		}
		close(fd);
	}
#endif
	exit(0);
}
#endif
