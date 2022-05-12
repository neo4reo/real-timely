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
#define FRAMES_TO_DISCARD_ON_WARMUP (8)
#define FRAMES_TO_ACQUIRE (10)
#define FRAMES_PER_SECOND (30)

// Format is used by a number of functions, so made as a file global
static struct v4l2_format video_format;

struct mmap_buffer_descriptor
{
  void *start;
  size_t length;
};

static char *device_name;
static int device_file_descriptor = -1;
struct mmap_buffer_descriptor *mmap_buffer_descriptors;
static unsigned int number_of_device_buffers;
static int out_buf;
static int force_format = 1;

static int frames_to_acquire = (FRAMES_TO_ACQUIRE);
int frame_number = -FRAMES_TO_DISCARD_ON_WARMUP;
unsigned char writeback_buffer[(1280 * 960)];

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

char pgm_file_header[] = "P5\n#9999999999 sec 9999999999 msec \n" HRES_STR " " VRES_STR "\n255\n";
char pgm_filename[] = "frames/test0000.pgm";

/**
 * @brief Write out a PGM file from the image buffer to disk.
 */
static void write_pgm_image_to_disk(const void *buffer_start,
                                    int buffer_length,
                                    unsigned int frame_number,
                                    struct timespec *timestamp_time)
{
  // Create the new image file and open it for writing
  snprintf(&pgm_filename[11], 9, "%04d", frame_number);
  strncat(&pgm_filename[15], ".pgm", 5);
  int pgm_file_descriptor = open(pgm_filename, O_WRONLY | O_NONBLOCK | O_CREAT, 00666);
  if (-1 == pgm_file_descriptor)
    print_error_number_and_exit("open()");

  // Write the file header
  snprintf(&pgm_file_header[4], 11, "%010d", (int)(timestamp_time->tv_sec));
  strncat(&pgm_file_header[14], " sec ", 5);
  snprintf(&pgm_file_header[19], 11, "%010d", (int)((timestamp_time->tv_nsec) / NANOSECONDS_PER_MILLSECOND));
  strncat(&pgm_file_header[29], " msec \n" HRES_STR " " VRES_STR "\n255\n", 19);
  // Subtract 1 from sizeof header because it includes the null terminator for the string
  int bytes_written = write(pgm_file_descriptor, pgm_file_header, sizeof(pgm_file_header) - 1);
  if (-1 == bytes_written)
    print_error_number_and_exit("write()");

  // Write the image raster
  int total_image_bytes_written = 0;
  do
  {
    bytes_written = write(pgm_file_descriptor, buffer_start, buffer_length);
    if (-1 == bytes_written)
      print_error_number_and_exit("write()");
    total_image_bytes_written += bytes_written;
  } while (total_image_bytes_written < buffer_length);

  // Print time
  get_current_monotonic_raw_time(&time_now);
  printf("Frame written to flash at %lf, %d, bytes\n", get_elapsed_time_in_seconds(&time_start, &time_now), total_image_bytes_written);

  // Close the file
  close(pgm_file_descriptor);
}

/**
 * @brief Convert image to different format and save to disk.
 */
static void process_image(const void *buffer_start, int buffer_length)
{
  // Record timestamp of this frame.
  struct timespec frame_time;
  get_current_realtime_time(&frame_time);

  printf("frame %d: ", frame_number);

  if (frame_number == 0)
    get_current_monotonic_raw_time(&time_start);

  // Dump frames
  if (video_format.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
    print_error_and_exit("Camera is not using YUYV format\n");

  // Skip processing warmup frames.
  if (frame_number < 0)
    return;

  // Prepare a PGM-format graymap image sourced form the Y pixel values of the
  // raw YUYV frames.
  unsigned char *buffer_bytes = (unsigned char *)buffer_start;
  for (int input_byte_index = 0, output_byte_index = 0;
       input_byte_index < buffer_length;
       input_byte_index = input_byte_index + 4, output_byte_index = output_byte_index + 2)
  {
    // Pixels are YU and YV alternating, so YUYV which is 4 bytes
    // We want YY which is 2 bytes
    // Y1 is first byte, Y2 is third byte
    writeback_buffer[output_byte_index] = buffer_bytes[input_byte_index];
    writeback_buffer[output_byte_index + 1] = buffer_bytes[input_byte_index + 2];
  }

  write_pgm_image_to_disk(writeback_buffer, (buffer_length / 2), frame_number, &frame_time);
}

/**
 * @brief Capture and process a frame from the video stream.
 */
static int capture_next_frame()
{
  // Dequeue a frame buffer
  struct v4l2_buffer v4l2_buffer_descriptor;
  CLEAR(v4l2_buffer_descriptor);
  v4l2_buffer_descriptor.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  v4l2_buffer_descriptor.memory = V4L2_MEMORY_MMAP;
  int result = signal_safe_ioctl(device_file_descriptor, VIDIOC_DQBUF, &v4l2_buffer_descriptor);

  // Make sure dequeue went okay
  if (result == -1)
  {
    if (errno == EAGAIN)
      return 0;
    if (errno == EIO)
      // Could ignore EIO, but drivers should only set for serious errors,
      // although some set for non-fatal errors too.
      return 0;
    print_error_number_and_exit("VIDIOC_DQBUF");
  }
  assert(v4l2_buffer_descriptor.index < number_of_device_buffers);

  // Process the frame
  process_image(mmap_buffer_descriptors[v4l2_buffer_descriptor.index].start,
                v4l2_buffer_descriptor.bytesused);

  // Re-enqueue the frame buffer
  if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_QBUF, &v4l2_buffer_descriptor))
    print_error_number_and_exit("VIDIOC_QBUF");

  return 1;
}

/**
 * @brief Capture the prescribed number of frames from the stream.
 */
static void capture_frames()
{
  // Set capture rate
  printf("Capturing frames at %u frames per second\n", FRAMES_PER_SECOND);
  struct timespec frame_capture_delay;
  frame_capture_delay.tv_sec = 0;
  frame_capture_delay.tv_nsec = NANOSECONDS_PER_SECOND / FRAMES_PER_SECOND;

  // Capture frames
  struct timespec nanosleep_time_remaining;
  while (frame_number < frames_to_acquire)
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

    if (capture_next_frame())
    {
      if (0 != nanosleep(&frame_capture_delay, &nanosleep_time_remaining))
        print_error_number_and_exit("nanosleep()");

      if (frame_number >= 0)
      {
        get_current_monotonic_raw_time(&time_now);
        double elapsed_time = get_elapsed_time_in_seconds(&time_start, &time_now);
        printf(" completed at %lf, @ %lf FPS\n", elapsed_time, (frame_number + 1) / elapsed_time);
      }
      else
        printf(" discarded\n");

      frame_number++;
    }
  }

  get_current_monotonic_raw_time(&time_stop);
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
    struct v4l2_buffer v4l2_buffer_descriptor;
    CLEAR(v4l2_buffer_descriptor);
    v4l2_buffer_descriptor.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buffer_descriptor.memory = V4L2_MEMORY_MMAP;
    v4l2_buffer_descriptor.index = index;

    printf("Enqueueing device buffer %d\n", index);
    if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_QBUF, &v4l2_buffer_descriptor))
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
    if (-1 == munmap(mmap_buffer_descriptors[index].start, mmap_buffer_descriptors[index].length))
      print_error_number_and_exit("munmap");

  free(mmap_buffer_descriptors);
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
  mmap_buffer_descriptors = calloc(number_of_device_buffers, sizeof(*mmap_buffer_descriptors));
  if (!mmap_buffer_descriptors)
    print_error_and_exit("Out of memory\n");

  // Initialize each allocated device buffer.
  // TODO NICK: Original code seemed to leave `number_of_device_buffers` at one too many. Check if this is an issue.
  for (unsigned int device_buffer_index = 0; device_buffer_index < number_of_device_buffers; ++device_buffer_index)
  {
    struct v4l2_buffer v4l2_buffer_descriptor;
    CLEAR(v4l2_buffer_descriptor);
    v4l2_buffer_descriptor.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buffer_descriptor.memory = V4L2_MEMORY_MMAP;
    v4l2_buffer_descriptor.index = device_buffer_index;

    if (-1 == signal_safe_ioctl(device_file_descriptor, VIDIOC_QUERYBUF, &v4l2_buffer_descriptor))
      print_error_number_and_exit("VIDIOC_QUERYBUF");

    mmap_buffer_descriptors[device_buffer_index].length = v4l2_buffer_descriptor.length;
    mmap_buffer_descriptors[device_buffer_index].start =
        mmap(NULL,
             v4l2_buffer_descriptor.length,
             PROT_READ | PROT_WRITE,
             MAP_SHARED,
             device_file_descriptor,
             v4l2_buffer_descriptor.m.offset);

    if (MAP_FAILED == mmap_buffer_descriptors[device_buffer_index].start)
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
          argv[0], device_name, frames_to_acquire);
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
      frames_to_acquire = strtol(optarg, NULL, 0);
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
  printf("Total capture time=%lf, for %d frames, %lf FPS\n", get_elapsed_time_in_seconds(&time_start, &time_stop), FRAMES_TO_ACQUIRE, ((double)FRAMES_TO_ACQUIRE / get_elapsed_time_in_seconds(&time_start, &time_stop)));

  stop_streaming();
  uninitialize_mmap();
  close_device();

  return 0;
}
