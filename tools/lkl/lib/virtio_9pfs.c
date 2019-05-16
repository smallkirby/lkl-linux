#include <string.h>
#include <lkl_host.h>
#include "virtio.h"
#include "endian.h"

struct virtio_9p_dev {
	struct virtio_dev dev;
	struct lkl_virtio_9p_config config;
	struct lkl_dev_9pfs_ops *ops;
	struct lkl_9pfs fs;
};

struct virtio_blk_req_trailer {
	uint8_t status;
};

static int fs9p_check_features(struct virtio_dev *dev)
{
	if (dev->driver_features == dev->device_features)
		return 0;

	return -LKL_EINVAL;
}

static int fs9p_enqueue(struct virtio_dev *dev, int q, struct virtio_req *req)
{
	struct virtio_9p_dev *fs9p_dev;
	int ret = 0;

	if (req->buf_count < 2) {
		lkl_printf("virtio_9pfs: no incoming buf\n");
		goto out;
	}

	fs9p_dev = container_of(dev, struct virtio_9p_dev, dev);
	ret = fs9p_dev->ops->request(fs9p_dev->fs, req->buf, req->buf_count);

out:
	virtio_req_complete(req, ret);
	return 0;
}

static struct virtio_dev_ops fs9p_ops = {
	.check_features = fs9p_check_features,
	.enqueue = fs9p_enqueue,
};


int lkl_9pfs_add(struct lkl_9pfs *fs)
{
	struct virtio_9p_dev *dev;
	int ret;

	dev = lkl_host_ops.mem_alloc(sizeof(*dev));
	if (!dev)
		return -LKL_ENOMEM;
	memset(dev, 0, sizeof(struct virtio_9p_dev));

	fs->dev = dev;

#define BIT(x) (1ULL << x)
	dev->dev.device_id = LKL_VIRTIO_ID_9P;
	dev->dev.vendor_id = 0;
	dev->dev.device_features = BIT(LKL_VIRTIO_9P_MOUNT_TAG);
	dev->dev.config_gen = 0;
	dev->dev.config_data = &dev->config;
	dev->dev.config_len = sizeof(dev->config);
	dev->dev.ops = &fs9p_ops;
	if (fs->ops)
		dev->ops = fs->ops;
	else
		dev->ops = &lkl_dev_9pfs_ops;
	dev->fs = *fs;

	ret = virtio_dev_setup(&dev->dev, 1, 32);
	if (ret)
		goto out_free;

	return dev->dev.virtio_mmio_id;

out_free:
	lkl_host_ops.mem_free(dev);

	return ret;
}

int lkl_9pfs_remove(struct lkl_9pfs fs)
{
	struct virtio_9p_dev *dev;
	int ret;

	dev = (struct virtio_9p_dev *)fs.dev;
	if (!dev)
		return -LKL_EINVAL;

	ret = virtio_dev_cleanup(&dev->dev);
	if (ret < 0)
		return ret;

	lkl_host_ops.mem_free(dev);

	return 0;
}
