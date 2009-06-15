/*
 * tiny fuser implementation
 *
 * Copyright 2004 Tony J. White
 *
 * May be distributed under the conditions of the
 * GNU Library General Public License
 *
 * Ported to be used by the bridge daemon.
 */

#include <dirent.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DOT_OR_DOTDOT(s) ((s)[0] == '.' && (!(s)[1] || ((s)[1] == '.' && !(s)[2])))
#define MAX_LINE 255

#define debug_print(...)

typedef struct inode_list {
	struct inode_list *next;
	ino_t inode;
	dev_t dev;
} inode_list;

typedef struct pid_list {
	struct pid_list *next;
	pid_t pid;
} pid_list;

struct dirent *de;

int pmanager(void);
static pid_list *add_pid(pid_list *plist, pid_t pid);
static int file_to_dev_inode(const char *filename, dev_t *dev, ino_t *inode);
static int kill_pid_list(pid_list *plist, int sig);
static pid_list *scan_proc_pids(inode_list *ilist);
static pid_list *scan_link(const char *lname, pid_t pid,
                                inode_list *ilist, pid_list *plist);
static pid_list *scan_dir_links(const char *dname, pid_t pid,
                                inode_list *ilist, pid_list *plist);
static int search_dev_inode(inode_list *ilist, dev_t dev, ino_t inode);
static pid_list *scan_pid_maps(const char *fname, pid_t pid,
				inode_list *ilist, pid_list *plist);
char *concat_subpath_file(const char *path, const char *f);
char *concat_path_file(const char *path, const char *filename);
char *last_char_is(const char *s, int c);

static inode_list *add_inode(inode_list *ilist, dev_t dev, ino_t inode)
{
	inode_list *curr = ilist;
	while (curr != NULL) {
		if (curr->inode == inode && curr->dev == dev)
			return ilist;
		curr = curr->next;
	}
	curr = malloc(sizeof(inode_list));
	curr->dev = dev;
	curr->inode = inode;
	curr->next = ilist;
	return curr;
}

static pid_list *add_pid(pid_list *plist, pid_t pid)
{
	pid_list *curr = plist;

	debug_print("\nAdd pid %x %d\t", plist, pid);
	while (curr != NULL) {
		if (curr->pid == pid)
			return plist;
		curr = curr->next;
	}
	curr = malloc(sizeof(pid_list));
	curr->pid = pid;
	curr->next = plist;
	return curr;
}

static int file_to_dev_inode(const char *filename, dev_t *dev, ino_t *inode)
{
	struct stat f_stat;

	debug_print("Actual folder: %s %s", buf, ptr);
	debug_print("file_to_dev_inode %s %x %x\n", filename, dev, inode);
	if (stat(filename, &f_stat)) {
		debug_print("file not found %s %x %x\n", filename, dev, inode);
		return 0;
	}
	*inode = f_stat.st_ino;
	*dev = f_stat.st_dev;
	return 1;
}

int pmanager(void)
{
	dev_t dev = 0;
	ino_t inode = 0;
	char *name = "/dev/DspBridge";
	inode_list *ilist = 0;
	pid_list *plist;

	if (!file_to_dev_inode(name, &dev, &inode))
		debug_print("Error Open %s\n", name);
	ilist = add_inode(ilist, dev, inode);
	plist = scan_proc_pids(ilist);

	/* Wait 1 second for processes to exit gracefully */
	sleep(1);
	kill_pid_list(plist, SIGKILL);

	/* TODO: replace arbitrary sleep and check if the process was killed */
	sleep(1);

	return 1;
}

static int kill_pid_list(pid_list *plist, int sig)
{
	pid_t mypid = getpid();
	int success = 1;

	debug_print("plist %x signal %d\n", plist, sig);
	while (plist != NULL) {
		if (plist->pid != mypid) {
			if (kill(plist->pid, sig) != 0) {
				debug_print("::pid killed %u\n",
						(unsigned)plist->pid);
				success = 0;
			}
		}
		plist = plist->next;
	}
	return success;
}

static pid_list *scan_proc_pids(inode_list *ilist)
{
	DIR *d;
	pid_t pid;
	pid_list *plist;

	chdir("/proc");
	d = opendir(".");
	if (!d)
		return NULL;

	plist = NULL;
	while ((de = readdir(d)) != NULL) {
		pid = (pid_t)strtoul(de->d_name, NULL, 10);
		debug_print("readdir: de %s, pid %d\n", de->d_name, pid);
		if (chdir(de->d_name) < 0 && pid <= 0)
			continue;
		debug_print("chdir: de->d_name %s\n", de->d_name);
		plist = scan_link("cwd", pid, ilist, plist);
		plist = scan_link("exe", pid, ilist, plist);
		plist = scan_link("root", pid, ilist, plist);
		plist = scan_dir_links("fd", pid, ilist, plist);
		plist = scan_dir_links("lib", pid, ilist, plist);
		plist = scan_dir_links("mmap", pid, ilist, plist);
		plist = scan_pid_maps("maps", pid, ilist, plist);
		chdir("/proc");
	}
	closedir(d);
	return plist;
}

static pid_list *scan_link(const char *lname, pid_t pid,
                                inode_list *ilist, pid_list *plist)
{
        ino_t inode;
        dev_t dev;

	chdir("/proc");
	chdir(de->d_name);
	debug_print("::scan link enter %s %x %x\n", lname, &dev, &inode);
        if (!file_to_dev_inode(lname, &dev, &inode)) {
		debug_print("file_to_dev_inode failed\n");
                return plist;
	}
        if (search_dev_inode(ilist, dev, inode)) {
		debug_print ("Search dev inode %x %x %x\t", ilist, dev, inode);
                plist = add_pid(plist, pid);
		debug_print("plist %x\n", plist);
	}
        return plist;
}

static pid_list *scan_dir_links(const char *dname, pid_t pid,
                                inode_list *ilist, pid_list *plist)
{
        DIR *d;
        char *lname;

	chdir("/proc");
	debug_print("scan dir lnks %s %d %x %x\n", dname, pid, ilist, plist);
        d = opendir(dname);
        if (!d)
                return plist;
        while ((de = readdir(d)) != NULL) {
                lname = concat_subpath_file(dname, de->d_name);
		debug_print("lname %s %s\n", lname, de->d_name);
                if (lname == NULL)
                        continue;
                plist = scan_link(lname, pid, ilist, plist);
                free(lname);
        }
        closedir(d);
        return plist;
}

static int search_dev_inode(inode_list *ilist, dev_t dev, ino_t inode)
{
	while (ilist) {
		if (ilist->dev == dev) {
			if (ilist->inode == inode)
				return 1;
		}
		ilist = ilist->next;
	}
	return 0;
}

static pid_list *scan_pid_maps(const char *fname, pid_t pid,
				inode_list *ilist, pid_list *plist)
{
	FILE *file;
	char line[MAX_LINE + 1];
	int major, minor;
	ino_t inode;
	long long uint64_inode;
	dev_t dev;

	chdir("/proc");
	chdir(de->d_name);
	file = fopen(fname, "r");
	if (!file)
		return plist;
	while (fgets(line, MAX_LINE, file)) {
		if (sscanf(line, "%*s %*s %*s %x:%x %llu",
				&major, &minor, &uint64_inode) != 3)
			continue;
		inode = uint64_inode;
		if (major == 0 && minor == 0 && inode == 0)
			continue;
		dev = makedev(major, minor);
		if (search_dev_inode(ilist, dev, inode))
			plist = add_pid(plist, pid);
	}
	fclose(file);
	return plist;
}

char *concat_subpath_file(const char *path, const char *f)
{
	if (f && DOT_OR_DOTDOT(f))
		return NULL;
	return concat_path_file(path, f);
}

char *concat_path_file(const char *path, const char *filename)
{
	char *lc;

	if (!path)
		path = "";
	lc = last_char_is(path, '/');
	while (*filename == '/')
		filename++;

	return (char *)printf("%s%s%s", path, (lc==NULL ? "/" : ""), filename);
}

char* last_char_is(const char *s, int c)
{
	if (s && *s) {
		size_t sz = strlen(s) - 1;
		s += sz;
		if ( (unsigned char)*s == c)
			return (char*)s;
	}
	return NULL;
}
