/*
 *  Adapted by Nick McCrea. Original preamble is below:
 *
 *  ---
 *
 *  Adapted by Sam Siewert for use with UVC web cameras and Bt878 frame
 *  grabber NTSC cameras to acquire digital video from a source,
 *  time-stamp each frame acquired, save to a PGM or PPM file.
 *
 *  The original code adapted was open source from V4L2 API and had the
 *  following use and incorporation policy:
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see http://linuxtv.org/docs.php for more information
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "../utils/error.h"
#include "../utils/time.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define HRES 640
#define VRES 480
#define HRES_STR "640"
#define VRES_STR "480"

#define DEVICE_BUFFERS_TO_REQUEST (6)
#define START_UP_FRAMES (8)
#define LAST_FRAMES (1)
#define CAPTURE_FRAMES (1800 + LAST_FRAMES)
#define FRAMES_TO_ACQUIRE (CAPTURE_FRAMES + START_UP_FRAMES + LAST_FRAMES)
#define FRAMES_PER_SECOND (30)

//#define COLOR_CONVERT_RGB
#define DUMP_FRAMES

// Format is used by a number of functions, so made as a file global
static struct v4l2_format video_format;

struct device_buffer_info
{
  void *start;
  size_t length;
};

static char *device_name;
static int device_file_descriptor = -1;
struct device_buffer_info *device_buffer_infos;
static unsigned int number_of_device_buffers;
static int out_buf;
static int force_format = 1;

static int frame_count = (FRAMES_TO_ACQUIRE);

static struct timespec time_now, time_start, time_stop;

/**
 * @brief Call `ioctl()`, retrying if the function call is interrupted by a
 * software signal.
 */
static int signal_safe_ioctl(int descriptor, int request, void *arg)
{
  int result;
  do
  {
    result = ioctl(descriptor, request, arg);
  } while (result == -1 && errno == EINTR);
  return result;
}

char ppm_header[] = "P6\n#9999999999 sec 9999999999 msec \n" HRES_STR " " VRES_STR "\n255\n";
char ppm_dumpname[] = "frames/test0000.ppm";

static void dump_ppm(const void *p, int size, unsigned int tag, struct timespec *time)
{
  int written, total, dumpfd;

  snprintf(&ppm_dumpname[11], 9, "%04d", tag);
  strncat(&ppm_dumpname[15], ".ppm", 5);
  dumpfd = open(ppm_dumpname, O_WRONLY | O_NONBLOCK | O_CREAT, 00666);

  snprintf(&ppm_header[4], 11, "%010d", (int)time->tv_sec);
  strncat(&ppm_header[14], " sec ", 5);
  snprintf(&ppm_header[19], 11, "%010d", (int)((time->tv_nsec) / 1000000));
  strncat(&ppm_header[29], " msec \n" HRES_STR " " VRES_STR "\n255\n", 19);

  // subtract 1 from sizeof header because it includes the null terminator for the string
  written = write(dumpfd, ppm_header, sizeof(ppm_header) - 1);

  total = 0;

  do
  {
    written = write(dumpfd, p, size);
    total += written;
  } while (total < size);

  clock_gettime(CLOCK_MONOTONIC, &time_now);
  printf("Frame written to flash at %lf, %d, bytes\n", get_elapsed_time_in_seconds(&time_start, &time_now), total);

  close(dumpfd);
}

char pgm_header[] = "P5\n#9999999999 sec 9999999999 msec \n" HRES_STR " " VRES_STR "\n255\n";
char pgm_dumpname[] = "frames/test0000.pgm";

static void dump_pgm(const void *p, int size, unsigned int tag, struct timespec *time)
{
  int written, total, dumpfd;

  snprintf(&pgm_dumpname[11], 9, "%04d", tag);
  strncat(&pgm_dumpname[15], ".pgm", 5);
  dumpfd = open(pgm_dumpname, O_WRONLY | O_NONBLOCK | O_CREAT, 00666);

  snprintf(&pgm_header[4], 11, "%010d", (int)time->tv_sec);
  strncat(&pgm_header[14], " sec ", 5);
  snprintf(&pgm_header[19], 11, "%010d", (int)((time->tv_nsec) / 1000000));
  strncat(&pgm_header[29], " msec \n" HRES_STR " " VRES_STR "\n255\n", 19);

  // subtract 1 from sizeof header because it includes the null terminator for the string
  written = write(dumpfd, pgm_header, sizeof(pgm_header) - 1);

  total = 0;

  do
  {
    written = write(dumpfd, p, size);
    total += written;
  } while (total < size);

  clock_gettime(CLOCK_MONOTONIC, &time_now);
  printf("Frame written to flash at %lf, %d, bytes\n", get_elapsed_time_in_seconds(&time_start, &time_now), total);

  close(dumpfd);
}

void yuv2rgb_float(float y, float u, float v,
                   unsigned char *r, unsigned char *g, unsigned char *b)
{
  float r_temp, g_temp, b_temp;

  // R = 1.164(Y-16) + 1.1596(V-128)
  r_temp = 1.164 * (y - 16.0) + 1.1596 * (v - 128.0);
  *r = r_temp > 255.0 ? 255 : (r_temp < 0.0 ? 0 : (unsigned char)r_temp);

  // G = 1.164(Y-16) - 0.813*(V-128) - 0.391*(U-128)
  g_temp = 1.164 * (y - 16.0) - 0.813 * (v - 128.0) - 0.391 * (u - 128.0);
  *g = g_temp > 255.0 ? 255 : (g_temp < 0.0 ? 0 : (unsigned char)g_temp);

  // B = 1.164*(Y-16) + 2.018*(U-128)
  b_temp = 1.164 * (y - 16.0) + 2.018 * (u - 128.0);
  *b = b_temp > 255.0 ? 255 : (b_temp < 0.0 ? 0 : (unsigned char)b_temp);
}

// This is probably the most acceptable conversion from camera YUYV to RGB
//
// Wikipedia has a good discussion on the details of various conversions and cites good references:
// http://en.wikipedia.org/wiki/YUV
//
// Also http://www.fourcc.org/yuv.php
//
// What's not clear without knowing more about the camera in question is how often U & V are sampled compared
// to Y.
//
// E.g. YUV444, which is equivalent to RGB, where both require 3 bytes for each pixel
//      YUV422, which we assume here, where there are 2 bytes for each pixel, with two Y samples for one U & V,
//              or as the name implies, 4Y and 2 UV pairs
//      YUV420, where for every 4 Ys, there is a single UV pair, 1.5 bytes for each pixel or 36 bytes for 24 pixels

void yuv2rgb(int y, int u, int v, unsigned char *r, unsigned char *g, unsigned char *b)
{
  int r1, g1, b1;

  // replaces floating point coefficients
  int c = y - 16, d = u - 128, e = v - 128;

  // Conversion that avoids floating point
  r1 = (298 * c + 409 * e + 128) >> 8;
  g1 = (298 * c - 100 * d - 208 * e + 128) >> 8;
  b1 = (298 * c + 516 * d + 128) >> 8;

  // Computed values may need clipping.
  if (r1 > 255)
    r1 = 255;
  if (g1 > 255)
    g1 = 255;
  if (b1 > 255)
    b1 = 255;

  if (r1 < 0)
    r1 = 0;
  if (g1 < 0)
    g1 = 0;
  if (b1 < 0)
    b1 = 0;

  *r = r1;
  *g = g1;
  *b = b1;
}

// always ignore first 8 frames
int frames_processed = -8;

unsigned char bigbuffer[(1280 * 960)];

static void process_image(const void *p, int size)
{
  int i, newi;
  struct timespec frame_time;
  unsigned char *pptr = (unsigned char *)p;

  // record when process was called
  clock_gettime(CLOCK_REALTIME, &frame_time);

  frames_processed++;
  printf("frame %d: ", frames_processed);

  if (frames_processed == 0)
    clock_gettime(CLOCK_MONOTONIC, &time_start);

#ifdef DUMP_FRAMES

  // This just dumps the frame to a file now, but you could replace with whatever image
  // processing you wish.
  //

  if (video_format.fmt.pix.pixelformat == V4L2_PIX_FMT_GREY)
  {
    printf("Dump graymap as-is size %d\n", size);
    dump_pgm(p, size, frames_processed, &frame_time);
  }

  else if (video_format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV)
  {

#if defined(COLOR_CONVERT_RGB)

    // Pixels are YU and YV alternating, so YUYV which is 4 bytes
    // We want RGB, so RGBRGB which is 6 bytes
    //
    for (i = 0, newi = 0; i < size; i = i + 4, newi = newi + 6)
    {
      y_temp = (int)pptr[i];
      u_temp = (int)pptr[i + 1];
      y2_temp = (int)pptr[i + 2];
      v_temp = (int)pptr[i + 3];
      yuv2rgb(y_temp, u_temp, v_temp, &bigbuffer[newi], &bigbuffer[newi + 1], &bigbuffer[newi + 2]);
      yuv2rgb(y2_temp, u_temp, v_temp, &bigbuffer[newi + 3], &bigbuffer[newi + 4], &bigbuffer[newi + 5]);
    }

    if (framecnt > -1)
    {
      dump_ppm(bigbuffer, ((size * 6) / 4), framecnt, &frame_time);
      printf("Dump YUYV converted to RGB size %d\n", size);
    }
#else

    // Pixels are YU and YV alternating, so YUYV which is 4 bytes
    // We want Y, so YY which is 2 bytes
    //
    for (i = 0, newi = 0; i < size; i = i + 4, newi = newi + 2)
    {
      // Y1=first byte and Y2=third byte
      bigbuffer[newi] = pptr[i];
      bigbuffer[newi + 1] = pptr[i + 2];
    }

    if (frames_processed > -1)
    {
      dump_pgm(bigbuffer, (size / 2), frames_processed, &frame_time);
      printf("Dump YUYV converted to YY size %d\n", size);
    }
#endif
  }

  else if (video_format.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB24)
  {
    printf("Dump RGB as-is size %d\n", size);
    dump_ppm(p, size, frames_processed, &frame_time);
  }
  else
  {
    printf("ERROR - unknown dump format\n");
  }

#endif
}

static int read_frame(void)
{
  struct v4l2_buffer buf;

  CLEAR(buf);

  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;

  if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_DQBUF, &buf))
  {
    switch (errno)
    {
    case EAGAIN:
      return 0;

    case EIO:
      /* Could ignore EIO, but drivers should only set for serious errors, although some set for
         non-fatal errors too.
       */
      return 0;

    default:
      printf("mmap failure\n");
      print_error_number_and_exit("VIDIOC_DQBUF");
    }
  }

  assert(buf.index < number_of_device_buffers);

  process_image(device_buffer_infos[buf.index].start, buf.bytesused);

  if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_QBUF, &buf))
    print_error_number_and_exit("VIDIOC_QBUF");

  // printf("R");
  return 1;
}

/**
 * @brief Capture the prescribed number of frames from the stream.
 */
static void capture_frames(void)
{
  // Set capture rate
  printf("Capturing frames at %u frames per second\n", FRAMES_PER_SECOND);
  struct timespec frame_capture_delay;
  frame_capture_delay.tv_sec = 0;
  frame_capture_delay.tv_nsec = NANOSECONDS_PER_SECOND / FRAMES_PER_SECOND;

  // Capture frames
  struct timespec nanosleep_time_remaining;
  unsigned int frames_remaining = frame_count;
  while (frames_remaining > 0)
  {
    // Prepare to select using the device file descriptor.
    fd_set file_descriptor_set;
    FD_ZERO(&file_descriptor_set);
    FD_SET(device_file_descriptor, &file_descriptor_set);

    // Await ready frames
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    int number_of_set_descriptors = select(device_file_descriptor + 1, &file_descriptor_set, NULL, NULL, &timeout);

    // Validate that select() succeeded.
    if (-1 == number_of_set_descriptors)
    {
      if (EINTR == errno)
        continue;
      print_error_number_and_exit("select()");
    }
    if (0 == number_of_set_descriptors)
      print_error_and_exit("`select()` timed out\n");

    if (read_frame())
    {
      if (nanosleep(&frame_capture_delay, &nanosleep_time_remaining) != 0)
        perror("nanosleep()");
      else
      {
        if (frames_processed > 1)
        {
          clock_gettime(CLOCK_MONOTONIC, &time_now);
          printf(" read at %lf, @ %lf FPS\n", get_elapsed_time_in_seconds(&time_start, &time_now), (double)(frames_processed + 1) / get_elapsed_time_in_seconds(&time_start, &time_now));
        }
        else
        {
          printf("at %lf\n", get_time_in_seconds(&time_now));
        }
      }

      frames_remaining--;
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &time_stop);
}

/**
 * @brief Stop streaming frames and turn off the camera.
 */
static void stop_streaming(void)
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_STREAMOFF, &type))
    print_error_number_and_exit("VIDIOC_STREAMOFF");
}

/**
 * @brief Turn on the camera and begin capturing frames.
 */
static void start_streaming()
{
  // Enqueue all of the device buffers
  for (unsigned int index = 0; index < number_of_device_buffers; ++index)
  {
    struct v4l2_buffer v4l2_buffer_description;
    CLEAR(v4l2_buffer_description);
    v4l2_buffer_description.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buffer_description.memory = V4L2_MEMORY_MMAP;
    v4l2_buffer_description.index = index;

    printf("Enqueueing device buffer %d\n", index);
    if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_QBUF, &v4l2_buffer_description))
      print_error_number_and_exit("VIDIOC_QBUF");
  }

  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_STREAMON, &type))
    print_error_number_and_exit("VIDIOC_STREAMON");
}

/**
 * @brief Relase all memory-mapping resources.
 */
static void uninitialize_mmap(void)
{
  for (unsigned int index = 0; index < number_of_device_buffers; ++index)
    if (-1 == munmap(device_buffer_infos[index].start, device_buffer_infos[index].length))
      print_error_number_and_exit("munmap");

  free(device_buffer_infos);
}

/**
 * @brief Initialize memory-mapped frame buffers for the video device to write
 * to.
 */
static void initialize_mmap(void)
{
  // Prepare the request
  struct v4l2_requestbuffers device_buffers_description;
  CLEAR(device_buffers_description);
  device_buffers_description.count = DEVICE_BUFFERS_TO_REQUEST;
  device_buffers_description.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  device_buffers_description.memory = V4L2_MEMORY_MMAP;

  // Request the buffers
  if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_REQBUFS, &device_buffers_description))
  {
    if (EINVAL == errno)
      print_error_and_exit("%s does not support memory mapping\n", device_name);
    else
      print_error_number_and_exit("VIDIOC_REQBUFS");
  }

  // Validate that enough buffers were reserved
  number_of_device_buffers = device_buffers_description.count;
  if (number_of_device_buffers < 2)
    print_error_and_exit("Insufficient buffer memory on %s\n", device_name);

  // Allocate memory for buffer information
  device_buffer_infos = calloc(number_of_device_buffers, sizeof(*device_buffer_infos));
  if (!device_buffer_infos)
    print_error_and_exit("Out of memory\n");

  // Initialize each allocated device buffer.
  // TODO NICK: Original code seemed to leave `number_of_device_buffers` at one too many. Check if this is an issue.
  for (unsigned int device_buffer_index = 0; device_buffer_index < number_of_device_buffers; ++device_buffer_index)
  {
    struct v4l2_buffer v4l2_buffer_description;
    CLEAR(v4l2_buffer_description);
    v4l2_buffer_description.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buffer_description.memory = V4L2_MEMORY_MMAP;
    v4l2_buffer_description.index = device_buffer_index;

    if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_QUERYBUF, &v4l2_buffer_description))
      print_error_number_and_exit("VIDIOC_QUERYBUF");

    device_buffer_infos[device_buffer_index].length = v4l2_buffer_description.length;
    device_buffer_infos[device_buffer_index].start =
        mmap(NULL,
             v4l2_buffer_description.length,
             PROT_READ | PROT_WRITE,
             MAP_SHARED,
             device_file_descriptor,
             v4l2_buffer_description.m.offset);

    if (MAP_FAILED == device_buffer_infos[device_buffer_index].start)
      print_error_number_and_exit("mmap");
  }
}

/**
 * @brief Validate the device's capabilities.
 */
static void validate_device_capabilies()
{
  struct v4l2_capability device_capabilities;

  if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_QUERYCAP, &device_capabilities))
  {
    if (errno == EINVAL)
      print_error_and_exit("%s is not a V4L2 device.\n", device_name);
    else
      print_error_number_and_exit("VIDIOC_QUERYCAP");
  }

  if (!(device_capabilities.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    print_error_and_exit("%s is no video capture device\n", device_name);

  if (!(device_capabilities.capabilities & V4L2_CAP_STREAMING))
    print_error_and_exit("%s does not support streaming i/o\n", device_name);
}

/**
 * @brief Configure the video output format.
 */
static void configure_device_format()
{
  // Check the video device's cropping capabilities.
  struct v4l2_cropcap crop_capabilies;
  CLEAR(crop_capabilies);
  crop_capabilies.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (0 != signal_safe_ioctl(device_file_descriptor, VIDIOC_CROPCAP, &crop_capabilies))
    print_error_number_and_exit("VIDIOC_CROPCAP");

  // Configure the device crop.
  struct v4l2_crop crop;
  crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  crop.c = crop_capabilies.defrect; // Sets crop to default.
  if (0 != signal_safe_ioctl(device_file_descriptor, VIDIOC_S_CROP, &crop))
  {
    switch (errno)
    {
    case EINVAL:
      print_error_and_exit("Device does not support cropping.\n");
      break;
    default:
      // According to the starter code, it appears to be okay to ignore other
      // errors.
      // print_error_number_and_exit("VIDIOC_S_CROP");
      break;
    }
  }

  // Configure the video format.
  CLEAR(video_format);
  video_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (force_format)
  {
    printf("Configuring custom device format.\n");

    // Frame resolution
    video_format.fmt.pix.width = HRES;
    video_format.fmt.pix.height = VRES;

    // Pixel encoding format
    video_format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

    // Interlacing formatting
    video_format.fmt.pix.field = V4L2_FIELD_NONE;

    if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_S_FMT, &video_format))
      print_error_number_and_exit("VIDIOC_S_FMT");
  }
  else
  {
    printf("Using default device format.\n");
    if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_G_FMT, &video_format))
      print_error_number_and_exit("VIDIOC_G_FMT");
  }

  // Buggy driver paranoia. Prevents bad byte alignment.
  unsigned int min = video_format.fmt.pix.width * 2;
  if (video_format.fmt.pix.bytesperline < min)
    video_format.fmt.pix.bytesperline = min;
  min = video_format.fmt.pix.bytesperline * video_format.fmt.pix.height;
  if (video_format.fmt.pix.sizeimage < min)
    video_format.fmt.pix.sizeimage = min;
}

/**
 * @brief Close the video device file.
 */
static void close_device()
{
  if (-1 == close(device_file_descriptor))
    print_error_number_and_exit("close");

  device_file_descriptor = -1;
}

/**
 * @brief Validate the video device.
 */
static void validate_device_name()
{
  struct stat st;

  // Validate device is found
  if (-1 == stat(device_name, &st))
    print_error_and_exit("Cannot identify '%s': %d, %s\n", device_name, errno, strerror(errno));

  // Validate device is correct type
  if (!S_ISCHR(st.st_mode))
    print_error_and_exit("%s is no device\n", device_name);
}

/**
 * @brief Open the video device file.
 */
static void open_device()
{
  device_file_descriptor = open(device_name, O_RDWR | O_NONBLOCK, 0);
  if (-1 == device_file_descriptor)
    print_error_and_exit("Cannot open '%s': %d, %s\n", device_name, errno, strerror(errno));
}

/**
 * @brief Print this file's usage.
 */
static void usage(FILE *fp, int argc, char **argv)
{
  fprintf(fp,
          "Usage: %s [options]\n\n"
          "Version 1.3\n"
          "Options:\n"
          "-d | --device name   Video device name [%s]\n"
          "-h | --help          Print this message\n"
          "-o | --output        Outputs stream to stdout\n"
          "-f | --format        Force format to 640x480 GREY\n"
          "-c | --count         Number of frames to grab [%i]\n"
          "",
          argv[0], device_name, frame_count);
}

static const char short_options[] = "d:hmruofc:";

static const struct option
    long_options[] = {
        {"device", required_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {"output", no_argument, NULL, 'o'},
        {"format", no_argument, NULL, 'f'},
        {"count", required_argument, NULL, 'c'},
        {0, 0, 0, 0}};

int main(int argc, char **argv)
{
  if (argc > 1)
    device_name = argv[1];
  else
    device_name = "/dev/video0";

  for (;;)
  {
    int idx;
    int c;

    c = getopt_long(argc, argv,
                    short_options, long_options, &idx);

    if (-1 == c)
      break;

    switch (c)
    {
    case 0: /* getopt_long() flag */
      break;

    case 'd':
      device_name = optarg;
      break;

    case 'h':
      usage(stdout, argc, argv);
      exit(EXIT_SUCCESS);

    case 'o':
      out_buf++;
      break;

    case 'f':
      force_format++;
      break;

    case 'c':
      errno = 0;
      frame_count = strtol(optarg, NULL, 0);
      if (errno)
        print_error_number_and_exit(optarg);
      break;

    default:
      usage(stderr, argc, argv);
      exit(EXIT_FAILURE);
    }
  }

  validate_device_name();
  open_device();
  validate_device_capabilies();
  configure_device_format();
  initialize_mmap();

  start_streaming();
  capture_frames();
  stop_streaming();

  printf("Total capture time=%lf, for %d frames, %lf FPS\n", get_elapsed_time_in_seconds(&time_start, &time_stop), CAPTURE_FRAMES + 1, ((double)CAPTURE_FRAMES / get_elapsed_time_in_seconds(&time_start, &time_stop)));

  uninitialize_mmap();
  close_device();

  // TODO NICK: Why is this here?
  fprintf(stderr, "\n");

  return 0;
}
