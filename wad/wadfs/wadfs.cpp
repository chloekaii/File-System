#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#define FUSE_USE_VERSION 26
#include "../libWad/Wad.cpp"
using namespace std;

// All functions use this source: https://maastaar.net/fuse/linux/filesystem/c/2019/09/28/writing-less-simple-yet-stupid-filesystem-using-FUSE-in-C/
static int do_getattr(const char *path, struct stat *st)
{
    memset(st, 0, sizeof(struct stat));
    Wad *wad = ((Wad *)fuse_get_context()->private_data);

    time_t current_time = time(NULL);
    printf("Current time: %ld\n", current_time);

    st->st_uid = getuid();     // The owner of the file/directory is the user who mounted the filesystem
    st->st_gid = getgid();     // The group of the file/directory is the same as the group of the user who mounted the filesystem
    st->st_atime = time(NULL); // The last "a"ccess of the file/directory is right now
    st->st_mtime = time(NULL); // The last "m"odification of the file/directory is right now

    if (wad->isDirectory(path))
    {
        st->st_mode = S_IFDIR | 0777;
        st->st_nlink = 2;
    }
    else if (wad->isContent(path) == 1)
    {
        st->st_mode = S_IFREG | 0777;
        st->st_nlink = 1;
        st->st_size = wad->getSize(path);
    }
    else
    {
        return -ENOENT; // Path does not exist
    }

    return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    filler(buffer, ".", NULL, 0);  // Current Directory
    filler(buffer, "..", NULL, 0); // Parent Directory

    Wad *wad = ((Wad *)fuse_get_context()->private_data);

    vector<string> directories;
    wad->getDirectory(path, &directories);

    for (const auto &directory : directories) // add in the directories from wad
    {
        filler(buffer, directory.c_str(), nullptr, 0);
    }

    return 0;
}

static int do_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
    Wad *wad = ((Wad *)fuse_get_context()->private_data);

    if (wad->isContent(path))
    {
        return wad->getContents(path, buffer, size, offset);
    }

    return -ENOENT; // the file doesn't exist
}

static int do_mkdir(const char *path, mode_t mode)
{
    Wad *wad = ((Wad *)fuse_get_context()->private_data);
    wad->createDirectory(path);

    return 0;
}

static int do_mknod(const char *path, mode_t mode, dev_t rdev)
{
    Wad *wad = ((Wad *)fuse_get_context()->private_data);
    wad->createFile(path);

    return 0;
}

static int do_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *info)
{
    Wad *wad = ((Wad *)fuse_get_context()->private_data);
    wad->writeToFile(path, buffer, size, offset);

    return size;
}

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .mknod = do_mknod,
    .mkdir = do_mkdir,
    .read = do_read,
    .write = do_write,
    .readdir = do_readdir,
};

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cout << "Not enough arguments." << endl;
    }

    string wadPath = argv[argc - 2];

    if (wadPath.at(0) != '/')
    {
        wadPath = string(get_current_dir_name()) + "/" + wadPath;
    }

    Wad *myWad = Wad::loadWad(wadPath);

    argv[argc - 2] = argv[argc - 1];
    argc--;

    return fuse_main(argc, argv, &operations, myWad);
}
