/******************************************************************************
 * Simple V4L2 interface to get a raw data buffer from a video source
 * and save it to a binary file.
 *
 * TO DO: re-test after video0 and video1 drivers are functional
 *
 * Shawn Quinn
 * June 2, 2016
 *
******************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>

using namespace std;

uint8_t *buffer;
struct v4l2_buffer buf = {0};
const int repeatCount = 2;  // this is used in a work around for
                            // the stream not turning off

int printCapabilities(int fd)
{
    int retVal = 0;
    struct v4l2_capability caps = {};

    //
    // query camera driver settings
    //
    retVal = ioctl(fd, VIDIOC_QUERYCAP, &caps);
    if (retVal < 0) {
        perror("Querying Capabilities");
        return 1;
    }
    cout << "Camera Driver Capabilities:\n"
         << "  Driver: " << caps.driver << endl
         << "  Card: " << caps.card << endl
         << "  Bus: " << caps.bus_info << endl
         << "  Version: " << ((caps.version>>16)&&0xff) << "."
         << ((caps.version>>24)&&0xff) << endl
         << showbase << internal << setfill('0')
         << "  Capabilities: " << std::hex << caps.capabilities
         << std::dec << endl;

    //
    // get the current camera settings
    //
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    retVal = ioctl(fd, VIDIOC_G_FMT, &fmt);
    if (retVal < 0) {
        perror("Getting Pixel Format");
        return 1;
    }
    char pixelFormat[5] = {0};
    strncpy(pixelFormat, (char *)&fmt.fmt.pix.pixelformat, sizeof(fmt.fmt.pix.pixelformat));
    cout << "Camera Parameters:" << endl
         << "  Width: " << fmt.fmt.pix.width << endl
         << "  Height: " << fmt.fmt.pix.height << endl
         << "  PixFmt: " << pixelFormat << endl
         << "  Field: " << fmt.fmt.pix.field << endl;

        return 0;
}

int initMmap(int fd)
{
    int retVal = 0;
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    //
    // request that a buffer be allocated internally on the camera
    //
    cout << "requesting buffers..." << endl;
    retVal = ioctl(fd, VIDIOC_REQBUFS, &req);
    if (retVal < 0) {
        perror("Requesting Buffer");
        return 1;
    }

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    //
    // query status of buffer to verify buffer before calling mmap
    //
    cout << "query buffer..." << endl;
    retVal = ioctl(fd, VIDIOC_QUERYBUF, &buf);
    if (retVal < 0) {
        perror("Querying Buffer");
        return 1;
    }

    //
    // map internal camera buffer to process memory space
    //
    cout << "mapping memory..." << endl;
    buffer = (uint8_t *)mmap (NULL, buf.length, PROT_READ | PROT_WRITE,
                              MAP_SHARED, fd, buf.m.offset);
    if ( buffer == MAP_FAILED) {
        cout << "mmap failed..." << endl;
    }
    cout << "Mapped buffer Length: " << buf.length << endl;

    return 0;
}

int captureImage(int fd)
{
    int retVal = 0;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    //
    // need to run through this sequence at least twice
    // to keep the driver happy...
    //
    for (int i = 0; i < repeatCount; ++i) {

        //
        // start streaming images to camera buffer
        //
        cout << "turning stream on..." << endl;
        retVal = ioctl(fd, VIDIOC_STREAMON, &buf.type);
        if (retVal < 0) {
            perror("Stream On");
            return 1;
        }

        //
        //  queue the buffer in camera memory
        //
        cout << "queueing the buffer..." << endl;
        retVal = ioctl(fd, VIDIOC_QBUF, &buf);
        if (retVal < 0) {
            perror("Query buffer");
            return 1;
        }

        //
        // check that the camera file descriptor is "ready"
        // before dequeing the buffer
        //
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {0};
        tv.tv_sec = 2;
        int r = select(fd+1, &fds, NULL, NULL, &tv);
        if(-1 == r)
        {
            perror("Waiting for frame");
            return 1;
        }

        //
        // dequeue the buffer from the camera memory
        //
        cout << "dequeing buffer..." << endl;
        retVal = ioctl(fd, VIDIOC_DQBUF, &buf);
        if (retVal < 0) {
            perror("Retrieving frame");
            return 1;
        }

        /**********************************************************************
        * tried inserting a streamoff here but the driver hangs
        **********************************************************************/

    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 2 || argc < 2) {
        cout << "usage:  enter video0, video1, or video2 after program name...\n";
        return 1;
    }

    int fd;

    cout << "opening file descriptor..." << endl;
    if (strcmp("video0", argv[1]) == 0) {
        fd = open("/dev/video0", O_RDWR);
    }
    else if (strcmp("video1", argv[1]) == 0) {
        fd = open("/dev/video1", O_RDWR);
    }
    else if (strcmp("video2", argv[1]) == 0) {
        fd = open("/dev/video2", O_RDWR);
    }

    if (fd == -1) {
        perror("Opening video device");
        return 1;
    }

    cout << "calling printCaps..." << endl;
    if(printCapabilities(fd)) {
        return 1;
    }

    cout << "calling initMmap..." << endl;
    if(initMmap(fd)) {
        return 1;
    }

    cout << "calling capture image..." << endl;
    if(captureImage(fd)) {
        return 1;
    }

    //
    // write the buffer to a file
    //
    cout << "writing " << buf.length << " bytes of data to file" << endl;
    FILE *pFile = NULL;

    if (strcmp("video0", argv[1]) == 0) {
        pFile = fopen ("video0Data.bin", "wb");
    }
    else if (strcmp("video1", argv[1]) == 0) {
        pFile = fopen ("video1Data.bin", "wb");
    }
    else if (strcmp("video2", argv[1]) == 0) {
        pFile = fopen ("video2Data.bin", "wb");
    }

    //pFile = fopen ("cameraData.bin", "wb");
    fwrite (buffer, 1, buf.length, pFile);
    fclose (pFile);

    //
    // un-map memory
    //
    cout << "um-mapping memory..." << endl;
    int retVal = 0;
    retVal = munmap(buffer, buf.length);
    if (retVal < 0) {
        perror("munmap");
    }

    close(fd);
    return 0;
}
