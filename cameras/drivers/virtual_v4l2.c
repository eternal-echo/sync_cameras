// src/modules/virtual_v4l2/virtual_v4l2.c

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#define WIDTH  640
#define HEIGHT 480
#define FPS    30
#define FRAME_SIZE (WIDTH * HEIGHT * 3) // RGB24

static struct v4l2_device vdev = {
    .name = "virtual_v4l2",
    .dev = NULL,
};
static struct video_device *vfd;
static struct timer_list frame_timer;
static int frame_idx = 0;
static DEFINE_SPINLOCK(frame_lock);
static unsigned long frame_lock_flags;

static u8 *frames[3]; // red, green, blue

static void fill_frames(void)
{
    int i;
    frames[0] = kzalloc(FRAME_SIZE, GFP_KERNEL); // red
    frames[1] = kzalloc(FRAME_SIZE, GFP_KERNEL); // green
    frames[2] = kzalloc(FRAME_SIZE, GFP_KERNEL); // blue
    for (i = 0; i < WIDTH * HEIGHT; i++) {
        frames[0][i*3+0] = 0xFF; // R
        frames[0][i*3+1] = 0x00; // G
        frames[0][i*3+2] = 0x00; // B

        frames[1][i*3+0] = 0x00;
        frames[1][i*3+1] = 0xFF;
        frames[1][i*3+2] = 0x00;

        frames[2][i*3+0] = 0x00;
        frames[2][i*3+1] = 0x00;
        frames[2][i*3+2] = 0xFF;
    }
}

// V4L2 ioctl实现
static int vidioc_querycap(struct file *file, void *priv, struct v4l2_capability *cap)
{
    strncpy(cap->driver, "virtual_v4l2", sizeof(cap->driver) - 1);
    cap->driver[sizeof(cap->driver) - 1] = '\0';
    
    strncpy(cap->card, "Virtual Camera", sizeof(cap->card) - 1);
    cap->card[sizeof(cap->card) - 1] = '\0';
    
    strncpy(cap->bus_info, "platform:virtual", sizeof(cap->bus_info) - 1);
    cap->bus_info[sizeof(cap->bus_info) - 1] = '\0';
    
    cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE;
    cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
    return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv, struct v4l2_fmtdesc *f)
{
    if (f->index > 0)
        return -EINVAL;
    f->pixelformat = V4L2_PIX_FMT_RGB24;
    strncpy(f->description, "RGB24", sizeof(f->description) - 1);
    f->description[sizeof(f->description) - 1] = '\0';
    return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
    f->fmt.pix.width = WIDTH;
    f->fmt.pix.height = HEIGHT;
    f->fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    f->fmt.pix.field = V4L2_FIELD_NONE;
    f->fmt.pix.bytesperline = WIDTH * 3;
    f->fmt.pix.sizeimage = FRAME_SIZE;
    f->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
    return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
    // 只支持固定格式
    return vidioc_g_fmt_vid_cap(file, priv, f);
}

static const struct v4l2_ioctl_ops virtual_v4l2_ioctl_ops = {
    .vidioc_querycap      = vidioc_querycap,
    .vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
    .vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
    .vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
};

// file_operations实现
static ssize_t virtual_v4l2_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    ssize_t ret = 0;
    u8 *frame_data;
    int current_frame;

    if (count > FRAME_SIZE)
        count = FRAME_SIZE;
    
    // 使用自旋锁获取当前帧索引
    spin_lock_irqsave(&frame_lock, frame_lock_flags);
    current_frame = frame_idx;
    frame_data = frames[current_frame];
    spin_unlock_irqrestore(&frame_lock, frame_lock_flags);
    
    // 在锁外复制数据，减少锁持有时间
    if (copy_to_user(buf, frame_data, count))
        ret = -EFAULT;
    else
        ret = count;
    
    return ret;
}

static int virtual_v4l2_open(struct file *file)
{
    return 0;
}

static int virtual_v4l2_release(struct file *file)
{
    return 0;
}

static const struct v4l2_file_operations virtual_v4l2_fops = {
    .owner = THIS_MODULE,
    .open = virtual_v4l2_open,
    .release = virtual_v4l2_release,
    .read = virtual_v4l2_read,
    .unlocked_ioctl = video_ioctl2,
};

static void frame_timer_func(struct timer_list *t)
{
    // 使用自旋锁代替mutex，因为timer回调可能在中断上下文执行
    spin_lock_irqsave(&frame_lock, frame_lock_flags);
    frame_idx = (frame_idx + 1) % 3;
    spin_unlock_irqrestore(&frame_lock, frame_lock_flags);

    mod_timer(&frame_timer, jiffies + msecs_to_jiffies(1000 / FPS));
}

static int __init virtual_v4l2_init(void)
{
    int ret;
    fill_frames();
    
    // 初始化 v4l2_device
    vdev.dev = NULL;  // 设置为 NULL 因为我们没有实际的设备
    ret = v4l2_device_register(NULL, &vdev);
    if (ret) {
        printk(KERN_ERR "virtual_v4l2: v4l2_device_register failed\n");
        goto err_free_frames;
    }
    
    // 分配并注册video_device
    vfd = video_device_alloc();
    if (!vfd) {
        ret = -ENOMEM;
        printk(KERN_ERR "virtual_v4l2: video_device_alloc failed\n");
        goto err_unreg_v4l2;
    }
    
    strncpy(vfd->name, "Virtual Camera", sizeof(vfd->name) - 1);
    vfd->name[sizeof(vfd->name) - 1] = '\0';
    vfd->fops = &virtual_v4l2_fops;
    vfd->ioctl_ops = &virtual_v4l2_ioctl_ops;
    vfd->release = video_device_release;
    vfd->v4l2_dev = &vdev;
    vfd->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE;
    
    ret = video_register_device(vfd, VFL_TYPE_VIDEO, -1);
    if (ret) {
        printk(KERN_ERR "virtual_v4l2: video_register_device failed\n");
        goto err_release_vfd;
    }
    
    // 设置定时器
    timer_setup(&frame_timer, frame_timer_func, 0);
    mod_timer(&frame_timer, jiffies + msecs_to_jiffies(1000 / FPS));
    
    printk(KERN_INFO "virtual_v4l2: device registered as /dev/video%d\n", vfd->num);
    return 0;

err_release_vfd:
    video_device_release(vfd);
err_unreg_v4l2:
    v4l2_device_unregister(&vdev);
err_free_frames:
    {
        int i;
        for (i = 0; i < 3; i++) {
            if (frames[i])
                kfree(frames[i]);
        }
    }
    return ret;
}

static void __exit virtual_v4l2_exit(void)
{
    int i;
    del_timer_sync(&frame_timer);
    if (vfd) {
        video_unregister_device(vfd);
        vfd = NULL;
    }
    v4l2_device_unregister(&vdev);
    for (i = 0; i < 3; i++) {
        if (frames[i]) {
            kfree(frames[i]);
            frames[i] = NULL;
        }
    }
    printk(KERN_INFO "virtual_v4l2: module unloaded\n");
}

module_init(virtual_v4l2_init);
module_exit(virtual_v4l2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("eternal-echo");
MODULE_DESCRIPTION("Virtual V4L2 Camera Driver");
MODULE_VERSION("0.1");