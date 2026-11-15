/*
 * SPDX-FileCopyrightText: 2026 Gursu Bilisim
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Primary audio HAL wrapper for the stock A530F (Android 9) audio HAL.
 *
 * The stock HAL was built against the Android 9 hardware/audio.h and
 * system/audio.h. The structures it hands out and reads differ from the
 * current ones in four places:
 *
 *  - audio_stream_in: Android 10 inserted set_microphone_direction and
 *    set_microphone_field_dimension before update_sink_metadata, so the
 *    current default implementation reads update_sink_metadata two slots past
 *    the end of the stock function table, inside the HAL's private stream data.
 *  - audio_stream_out and audio_hw_device: newer function pointers were
 *    appended; the stock structures end where they begin, so the
 *    implementation would read the HAL's private data for them.
 *  - audio_config: audio_offload_info_t grew by 12 bytes, which moves
 *    frame_count.
 *  - audio_port_config: a flags field was inserted before ext, and the device
 *    ext gained a field. record_track_metadata gained two fields.
 *
 * This module exposes full, current-layout device and stream structures,
 * forwards every call that the Android 9 API had to the stock HAL with its own
 * structures, converts the changed types, and leaves everything newer NULL.
 */

#define LOG_TAG "audio_hw_shim"

#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <log/log.h>

#define STOCK_HAL_PATH "/vendor/lib/hw/audio.stock.exynos7885.so"

/* ------------------------------------------------------------------------ */
/* Android 9 layouts                                                         */
/* ------------------------------------------------------------------------ */

typedef struct {
    uint16_t version;
    uint16_t size;
    uint32_t sample_rate;
    audio_channel_mask_t channel_mask;
    audio_format_t format;
    audio_stream_type_t stream_type;
    uint32_t bit_rate;
    int64_t duration_us;
    bool has_video;
    bool is_streaming;
    uint32_t bit_width;
    uint32_t offload_buffer_size;
    audio_usage_t usage;
} __attribute__((aligned(8))) legacy_offload_info_t;

struct __attribute__((aligned(8))) legacy_config {
    uint32_t sample_rate;
    audio_channel_mask_t channel_mask;
    audio_format_t format;
    legacy_offload_info_t offload_info;
    uint32_t frame_count;
};

struct legacy_port_config_device_ext {
    audio_module_handle_t hw_module;
    audio_devices_t type;
    char address[AUDIO_DEVICE_MAX_ADDRESS_LEN];
};

struct legacy_port_config {
    audio_port_handle_t id;
    audio_port_role_t role;
    audio_port_type_t type;
    unsigned int config_mask;
    unsigned int sample_rate;
    audio_channel_mask_t channel_mask;
    audio_format_t format;
    struct audio_gain_config gain;
    union {
        struct legacy_port_config_device_ext device;
        struct audio_port_config_mix_ext mix;
        struct audio_port_config_session_ext session;
    } ext;
};

typedef struct {
    audio_source_t source;
    float gain;
} legacy_record_track_metadata_t;

struct legacy_sink_metadata {
    size_t track_count;
    legacy_record_track_metadata_t *tracks;
};

struct legacy_stream_in {
    struct audio_stream common;
    int (*set_gain)(struct audio_stream_in *stream, float gain);
    ssize_t (*read)(struct audio_stream_in *stream, void *buffer, size_t bytes);
    uint32_t (*get_input_frames_lost)(struct audio_stream_in *stream);
    int (*get_capture_position)(const struct audio_stream_in *stream, int64_t *frames,
                                int64_t *time);
    int (*start)(const struct audio_stream_in *stream);
    int (*stop)(const struct audio_stream_in *stream);
    int (*create_mmap_buffer)(const struct audio_stream_in *stream, int32_t min_size_frames,
                              void *info);
    int (*get_mmap_position)(const struct audio_stream_in *stream, void *position);
    int (*get_active_microphones)(const struct audio_stream_in *stream,
                                  struct audio_microphone_characteristic_t *mic_array,
                                  size_t *mic_count);
    void (*update_sink_metadata)(struct audio_stream_in *stream,
                                 const struct legacy_sink_metadata *sink_metadata);
};

/* audio_stream_out: the Android 9 table is a prefix of the current one and
 * ends with update_source_metadata, whose argument did not change. */
typedef struct audio_stream_out legacy_stream_out;

struct legacy_device {
    struct hw_device_t common;
    uint32_t (*get_supported_devices)(const struct audio_hw_device *dev);
    int (*init_check)(const struct audio_hw_device *dev);
    int (*set_voice_volume)(struct audio_hw_device *dev, float volume);
    int (*set_master_volume)(struct audio_hw_device *dev, float volume);
    int (*get_master_volume)(struct audio_hw_device *dev, float *volume);
    int (*set_mode)(struct audio_hw_device *dev, audio_mode_t mode);
    int (*set_mic_mute)(struct audio_hw_device *dev, bool state);
    int (*get_mic_mute)(const struct audio_hw_device *dev, bool *state);
    int (*set_parameters)(struct audio_hw_device *dev, const char *kv_pairs);
    char *(*get_parameters)(const struct audio_hw_device *dev, const char *keys);
    size_t (*get_input_buffer_size)(const struct audio_hw_device *dev,
                                    const struct legacy_config *config);
    int (*open_output_stream)(struct audio_hw_device *dev, audio_io_handle_t handle,
                              audio_devices_t devices, audio_output_flags_t flags,
                              struct legacy_config *config, legacy_stream_out **stream_out,
                              const char *address);
    void (*close_output_stream)(struct audio_hw_device *dev, legacy_stream_out *stream_out);
    int (*open_input_stream)(struct audio_hw_device *dev, audio_io_handle_t handle,
                             audio_devices_t devices, struct legacy_config *config,
                             struct legacy_stream_in **stream_in, audio_input_flags_t flags,
                             const char *address, audio_source_t source);
    void (*close_input_stream)(struct audio_hw_device *dev, struct legacy_stream_in *stream_in);
    int (*get_microphones)(const struct audio_hw_device *dev,
                           struct audio_microphone_characteristic_t *mic_array, size_t *mic_count);
    int (*dump)(const struct audio_hw_device *dev, int fd);
    int (*set_master_mute)(struct audio_hw_device *dev, bool mute);
    int (*get_master_mute)(struct audio_hw_device *dev, bool *mute);
    int (*create_audio_patch)(struct audio_hw_device *dev, unsigned int num_sources,
                              const struct legacy_port_config *sources, unsigned int num_sinks,
                              const struct legacy_port_config *sinks,
                              audio_patch_handle_t *handle);
    int (*release_audio_patch)(struct audio_hw_device *dev, audio_patch_handle_t handle);
    int (*get_audio_port)(struct audio_hw_device *dev, void *port);
    int (*set_audio_port_config)(struct audio_hw_device *dev,
                                 const struct legacy_port_config *config);
};

/* The two inserted input stream slots: the stock update_sink_metadata sits
 * where the current set_microphone_direction is. */
_Static_assert(offsetof(struct legacy_stream_in, update_sink_metadata) ==
                       offsetof(struct audio_stream_in, set_microphone_direction),
               "audio_stream_in layout");
_Static_assert(offsetof(struct legacy_device, set_audio_port_config) ==
                       offsetof(struct audio_hw_device, set_audio_port_config),
               "audio_hw_device layout");
_Static_assert(offsetof(struct legacy_config, frame_count) + 16 ==
                       offsetof(struct audio_config, frame_count),
               "audio_config layout");
_Static_assert(offsetof(struct legacy_port_config, ext) + sizeof(union audio_io_flags) ==
                       offsetof(struct audio_port_config, ext),
               "audio_port_config layout");

/* ------------------------------------------------------------------------ */
/* Wrapper objects                                                           */
/* ------------------------------------------------------------------------ */

struct shim_device {
    struct audio_hw_device dev; /* must be first */
    struct legacy_device *stock;
};

struct shim_stream_in {
    struct audio_stream_in in; /* must be first */
    struct legacy_stream_in *stock;
};

struct shim_stream_out {
    struct audio_stream_out out; /* must be first */
    legacy_stream_out *stock;
};

#define SDEV(d) (((struct shim_device *)(d))->stock)
#define SDEVP(d) ((struct audio_hw_device *)SDEV(d))
#define SIN(s) (((struct shim_stream_in *)(s))->stock)
#define SINP(s) ((struct audio_stream_in *)SIN(s))
#define SOUT(s) (((struct shim_stream_out *)(s))->stock)

/* ------------------------------------------------------------------------ */
/* Type conversion                                                           */
/* ------------------------------------------------------------------------ */

static void config_to_legacy(const struct audio_config *src, struct legacy_config *dst) {
    memset(dst, 0, sizeof(*dst));
    dst->sample_rate = src->sample_rate;
    dst->channel_mask = src->channel_mask;
    dst->format = src->format;
    /* The Android 9 fields are a prefix of the current offload info. */
    memcpy(&dst->offload_info, &src->offload_info, sizeof(dst->offload_info));
    dst->offload_info.version = AUDIO_MAKE_OFFLOAD_INFO_VERSION(0, 1);
    dst->offload_info.size = sizeof(dst->offload_info);
    dst->frame_count = src->frame_count;
}

static void config_from_legacy(const struct legacy_config *src, struct audio_config *dst) {
    dst->sample_rate = src->sample_rate;
    dst->channel_mask = src->channel_mask;
    dst->format = src->format;
    dst->frame_count = src->frame_count;
}

static void port_config_to_legacy(const struct audio_port_config *src,
                                  struct legacy_port_config *dst) {
    memset(dst, 0, sizeof(*dst));
    dst->id = src->id;
    dst->role = src->role;
    dst->type = src->type;
    dst->config_mask = src->config_mask & ~AUDIO_PORT_CONFIG_FLAGS;
    dst->sample_rate = src->sample_rate;
    dst->channel_mask = src->channel_mask;
    dst->format = src->format;
    dst->gain = src->gain;
    switch (src->type) {
        case AUDIO_PORT_TYPE_DEVICE:
            dst->ext.device.hw_module = src->ext.device.hw_module;
            dst->ext.device.type = src->ext.device.type;
            memcpy(dst->ext.device.address, src->ext.device.address,
                   sizeof(dst->ext.device.address));
            break;
        case AUDIO_PORT_TYPE_MIX:
            dst->ext.mix = src->ext.mix;
            break;
        case AUDIO_PORT_TYPE_SESSION:
            dst->ext.session = src->ext.session;
            break;
        default:
            break;
    }
}

/* ------------------------------------------------------------------------ */
/* Stream common (one set for each direction)                                */
/* ------------------------------------------------------------------------ */

#define DEFINE_COMMON(dir, STOCK)                                                               \
    static uint32_t dir##_get_sample_rate(const struct audio_stream *s) {                      \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->get_sample_rate(c);                                                          \
    }                                                                                          \
    static int dir##_set_sample_rate(struct audio_stream *s, uint32_t rate) {                  \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->set_sample_rate(c, rate);                                                    \
    }                                                                                          \
    static size_t dir##_get_buffer_size(const struct audio_stream *s) {                        \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->get_buffer_size(c);                                                          \
    }                                                                                          \
    static audio_channel_mask_t dir##_get_channels(const struct audio_stream *s) {             \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->get_channels(c);                                                             \
    }                                                                                          \
    static audio_format_t dir##_get_format(const struct audio_stream *s) {                     \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->get_format(c);                                                               \
    }                                                                                          \
    static int dir##_set_format(struct audio_stream *s, audio_format_t format) {               \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->set_format(c, format);                                                       \
    }                                                                                          \
    static int dir##_standby(struct audio_stream *s) {                                         \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->standby(c);                                                                  \
    }                                                                                          \
    static int dir##_dump(const struct audio_stream *s, int fd) {                              \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->dump(c, fd);                                                                 \
    }                                                                                          \
    static audio_devices_t dir##_get_device(const struct audio_stream *s) {                    \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->get_device(c);                                                               \
    }                                                                                          \
    static int dir##_set_device(struct audio_stream *s, audio_devices_t device) {              \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->set_device(c, device);                                                       \
    }                                                                                          \
    static int dir##_set_parameters(struct audio_stream *s, const char *kv_pairs) {            \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->set_parameters(c, kv_pairs);                                                 \
    }                                                                                          \
    static char *dir##_get_parameters(const struct audio_stream *s, const char *keys) {        \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->get_parameters(c, keys);                                                     \
    }                                                                                          \
    static int dir##_add_audio_effect(const struct audio_stream *s, effect_handle_t effect) {  \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->add_audio_effect(c, effect);                                                 \
    }                                                                                          \
    static int dir##_remove_audio_effect(const struct audio_stream *s,                         \
                                         effect_handle_t effect) {                             \
        struct audio_stream *c = &STOCK(s)->common;                                            \
        return c->remove_audio_effect(c, effect);                                              \
    }                                                                                          \
    static void dir##_fill_common(struct audio_stream *w, const struct audio_stream *c) {      \
        w->get_sample_rate = c->get_sample_rate ? dir##_get_sample_rate : NULL;                \
        w->set_sample_rate = c->set_sample_rate ? dir##_set_sample_rate : NULL;                \
        w->get_buffer_size = c->get_buffer_size ? dir##_get_buffer_size : NULL;                \
        w->get_channels = c->get_channels ? dir##_get_channels : NULL;                         \
        w->get_format = c->get_format ? dir##_get_format : NULL;                               \
        w->set_format = c->set_format ? dir##_set_format : NULL;                               \
        w->standby = c->standby ? dir##_standby : NULL;                                        \
        w->dump = c->dump ? dir##_dump : NULL;                                                 \
        w->get_device = c->get_device ? dir##_get_device : NULL;                               \
        w->set_device = c->set_device ? dir##_set_device : NULL;                               \
        w->set_parameters = c->set_parameters ? dir##_set_parameters : NULL;                   \
        w->get_parameters = c->get_parameters ? dir##_get_parameters : NULL;                   \
        w->add_audio_effect = c->add_audio_effect ? dir##_add_audio_effect : NULL;             \
        w->remove_audio_effect = c->remove_audio_effect ? dir##_remove_audio_effect : NULL;    \
    }

DEFINE_COMMON(in, SIN)
DEFINE_COMMON(out, SOUT)

/* ------------------------------------------------------------------------ */
/* Input stream                                                              */
/* ------------------------------------------------------------------------ */

static int in_set_gain(struct audio_stream_in *s, float gain) {
    return SIN(s)->set_gain(SINP(s), gain);
}

static ssize_t in_read(struct audio_stream_in *s, void *buffer, size_t bytes) {
    return SIN(s)->read(SINP(s), buffer, bytes);
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *s) {
    return SIN(s)->get_input_frames_lost(SINP(s));
}

static int in_get_capture_position(const struct audio_stream_in *s, int64_t *frames,
                                   int64_t *time) {
    return SIN(s)->get_capture_position(SINP(s), frames, time);
}

static int in_get_active_microphones(const struct audio_stream_in *s,
                                     struct audio_microphone_characteristic_t *mic_array,
                                     size_t *mic_count) {
    return SIN(s)->get_active_microphones(SINP(s), mic_array, mic_count);
}

static void in_update_sink_metadata(struct audio_stream_in *s,
                                    const struct sink_metadata *sink_metadata) {
    struct legacy_sink_metadata legacy = {0, NULL};
    if (sink_metadata != NULL && sink_metadata->track_count > 0) {
        legacy.tracks = calloc(sink_metadata->track_count, sizeof(*legacy.tracks));
        if (legacy.tracks == NULL) return;
        for (size_t i = 0; i < sink_metadata->track_count; i++) {
            legacy.tracks[i].source = sink_metadata->tracks[i].source;
            legacy.tracks[i].gain = sink_metadata->tracks[i].gain;
        }
        legacy.track_count = sink_metadata->track_count;
    }
    SIN(s)->update_sink_metadata(SINP(s), &legacy);
    free(legacy.tracks);
}

static struct audio_stream_in *wrap_stream_in(struct legacy_stream_in *stock) {
    struct shim_stream_in *w = calloc(1, sizeof(*w));
    if (w == NULL) return NULL;
    w->stock = stock;
    in_fill_common(&w->in.common, &stock->common);
    w->in.set_gain = stock->set_gain ? in_set_gain : NULL;
    w->in.read = stock->read ? in_read : NULL;
    w->in.get_input_frames_lost = stock->get_input_frames_lost ? in_get_input_frames_lost : NULL;
    w->in.get_capture_position = stock->get_capture_position ? in_get_capture_position : NULL;
    /* MMAP (start, stop, create_mmap_buffer, get_mmap_position) stays NULL:
     * audio_mmap_buffer_info gained a field after Android 9. */
    w->in.get_active_microphones =
            stock->get_active_microphones ? in_get_active_microphones : NULL;
    w->in.update_sink_metadata = stock->update_sink_metadata ? in_update_sink_metadata : NULL;
    return &w->in;
}

/* ------------------------------------------------------------------------ */
/* Output stream                                                             */
/* ------------------------------------------------------------------------ */

static uint32_t out_get_latency(const struct audio_stream_out *s) {
    return SOUT(s)->get_latency(SOUT(s));
}

static int out_set_volume(struct audio_stream_out *s, float left, float right) {
    return SOUT(s)->set_volume(SOUT(s), left, right);
}

static ssize_t out_write(struct audio_stream_out *s, const void *buffer, size_t bytes) {
    return SOUT(s)->write(SOUT(s), buffer, bytes);
}

static int out_get_render_position(const struct audio_stream_out *s, uint32_t *dsp_frames) {
    return SOUT(s)->get_render_position(SOUT(s), dsp_frames);
}

static int out_get_next_write_timestamp(const struct audio_stream_out *s, int64_t *timestamp) {
    return SOUT(s)->get_next_write_timestamp(SOUT(s), timestamp);
}

static int out_set_callback(struct audio_stream_out *s, stream_callback_t callback,
                            void *cookie) {
    return SOUT(s)->set_callback(SOUT(s), callback, cookie);
}

static int out_pause(struct audio_stream_out *s) {
    return SOUT(s)->pause(SOUT(s));
}

static int out_resume(struct audio_stream_out *s) {
    return SOUT(s)->resume(SOUT(s));
}

static int out_drain(struct audio_stream_out *s, audio_drain_type_t type) {
    return SOUT(s)->drain(SOUT(s), type);
}

static int out_flush(struct audio_stream_out *s) {
    return SOUT(s)->flush(SOUT(s));
}

static int out_get_presentation_position(const struct audio_stream_out *s, uint64_t *frames,
                                         struct timespec *timestamp) {
    return SOUT(s)->get_presentation_position(SOUT(s), frames, timestamp);
}

static void out_update_source_metadata(struct audio_stream_out *s,
                                       const struct source_metadata *source_metadata) {
    SOUT(s)->update_source_metadata(SOUT(s), source_metadata);
}

static struct audio_stream_out *wrap_stream_out(legacy_stream_out *stock) {
    struct shim_stream_out *w = calloc(1, sizeof(*w));
    if (w == NULL) return NULL;
    w->stock = stock;
    out_fill_common(&w->out.common, &stock->common);
    w->out.get_latency = stock->get_latency ? out_get_latency : NULL;
    w->out.set_volume = stock->set_volume ? out_set_volume : NULL;
    w->out.write = stock->write ? out_write : NULL;
    w->out.get_render_position = stock->get_render_position ? out_get_render_position : NULL;
    w->out.get_next_write_timestamp =
            stock->get_next_write_timestamp ? out_get_next_write_timestamp : NULL;
    w->out.set_callback = stock->set_callback ? out_set_callback : NULL;
    w->out.pause = stock->pause ? out_pause : NULL;
    w->out.resume = stock->resume ? out_resume : NULL;
    w->out.drain = stock->drain ? out_drain : NULL;
    w->out.flush = stock->flush ? out_flush : NULL;
    w->out.get_presentation_position =
            stock->get_presentation_position ? out_get_presentation_position : NULL;
    /* MMAP stays NULL, as for input. */
    w->out.update_source_metadata =
            stock->update_source_metadata ? out_update_source_metadata : NULL;
    return &w->out;
}

/* ------------------------------------------------------------------------ */
/* Device                                                                    */
/* ------------------------------------------------------------------------ */

static uint32_t adev_get_supported_devices(const struct audio_hw_device *d) {
    return SDEV(d)->get_supported_devices(SDEVP(d));
}

static int adev_init_check(const struct audio_hw_device *d) {
    return SDEV(d)->init_check(SDEVP(d));
}

static int adev_set_voice_volume(struct audio_hw_device *d, float volume) {
    return SDEV(d)->set_voice_volume(SDEVP(d), volume);
}

static int adev_set_master_volume(struct audio_hw_device *d, float volume) {
    return SDEV(d)->set_master_volume(SDEVP(d), volume);
}

static int adev_get_master_volume(struct audio_hw_device *d, float *volume) {
    return SDEV(d)->get_master_volume(SDEVP(d), volume);
}

static int adev_set_mode(struct audio_hw_device *d, audio_mode_t mode) {
    return SDEV(d)->set_mode(SDEVP(d), mode);
}

static int adev_set_mic_mute(struct audio_hw_device *d, bool state) {
    return SDEV(d)->set_mic_mute(SDEVP(d), state);
}

static int adev_get_mic_mute(const struct audio_hw_device *d, bool *state) {
    return SDEV(d)->get_mic_mute(SDEVP(d), state);
}

static int adev_set_parameters(struct audio_hw_device *d, const char *kv_pairs) {
    return SDEV(d)->set_parameters(SDEVP(d), kv_pairs);
}

static char *adev_get_parameters(const struct audio_hw_device *d, const char *keys) {
    return SDEV(d)->get_parameters(SDEVP(d), keys);
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *d,
                                         const struct audio_config *config) {
    struct legacy_config legacy;
    config_to_legacy(config, &legacy);
    return SDEV(d)->get_input_buffer_size(SDEVP(d), &legacy);
}

static int adev_open_output_stream(struct audio_hw_device *d, audio_io_handle_t handle,
                                   audio_devices_t devices, audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out, const char *address) {
    struct legacy_config legacy;
    legacy_stream_out *stock = NULL;
    config_to_legacy(config, &legacy);
    int ret = SDEV(d)->open_output_stream(SDEVP(d), handle, devices, flags, &legacy, &stock,
                                          address);
    config_from_legacy(&legacy, config);
    *stream_out = NULL;
    if (ret != 0 || stock == NULL) return ret != 0 ? ret : -ENOMEM;
    *stream_out = wrap_stream_out(stock);
    if (*stream_out == NULL) {
        SDEV(d)->close_output_stream(SDEVP(d), stock);
        return -ENOMEM;
    }
    return 0;
}

static void adev_close_output_stream(struct audio_hw_device *d, struct audio_stream_out *s) {
    if (s == NULL) return;
    SDEV(d)->close_output_stream(SDEVP(d), SOUT(s));
    free(s);
}

static int adev_open_input_stream(struct audio_hw_device *d, audio_io_handle_t handle,
                                  audio_devices_t devices, struct audio_config *config,
                                  struct audio_stream_in **stream_in, audio_input_flags_t flags,
                                  const char *address, audio_source_t source) {
    struct legacy_config legacy;
    struct legacy_stream_in *stock = NULL;
    config_to_legacy(config, &legacy);
    int ret = SDEV(d)->open_input_stream(SDEVP(d), handle, devices, &legacy, &stock, flags,
                                         address, source);
    config_from_legacy(&legacy, config);
    *stream_in = NULL;
    if (ret != 0 || stock == NULL) return ret != 0 ? ret : -ENOMEM;
    *stream_in = wrap_stream_in(stock);
    if (*stream_in == NULL) {
        SDEV(d)->close_input_stream(SDEVP(d), stock);
        return -ENOMEM;
    }
    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *d, struct audio_stream_in *s) {
    if (s == NULL) return;
    SDEV(d)->close_input_stream(SDEVP(d), SIN(s));
    free(s);
}

static int adev_get_microphones(const struct audio_hw_device *d,
                                struct audio_microphone_characteristic_t *mic_array,
                                size_t *mic_count) {
    return SDEV(d)->get_microphones(SDEVP(d), mic_array, mic_count);
}

static int adev_dump(const struct audio_hw_device *d, int fd) {
    return SDEV(d)->dump(SDEVP(d), fd);
}

static int adev_set_master_mute(struct audio_hw_device *d, bool mute) {
    return SDEV(d)->set_master_mute(SDEVP(d), mute);
}

static int adev_get_master_mute(struct audio_hw_device *d, bool *mute) {
    return SDEV(d)->get_master_mute(SDEVP(d), mute);
}

static int adev_create_audio_patch(struct audio_hw_device *d, unsigned int num_sources,
                                   const struct audio_port_config *sources,
                                   unsigned int num_sinks, const struct audio_port_config *sinks,
                                   audio_patch_handle_t *handle) {
    if (SDEV(d)->create_audio_patch == NULL) return -ENOSYS;
    struct legacy_port_config *ls = calloc(num_sources ? num_sources : 1, sizeof(*ls));
    struct legacy_port_config *lk = calloc(num_sinks ? num_sinks : 1, sizeof(*lk));
    int ret = -ENOMEM;
    if (ls != NULL && lk != NULL) {
        for (unsigned int i = 0; i < num_sources; i++) port_config_to_legacy(&sources[i], &ls[i]);
        for (unsigned int i = 0; i < num_sinks; i++) port_config_to_legacy(&sinks[i], &lk[i]);
        ret = SDEV(d)->create_audio_patch(SDEVP(d), num_sources, ls, num_sinks, lk, handle);
    }
    free(ls);
    free(lk);
    return ret;
}

static int adev_release_audio_patch(struct audio_hw_device *d, audio_patch_handle_t handle) {
    if (SDEV(d)->release_audio_patch == NULL) return -ENOSYS;
    return SDEV(d)->release_audio_patch(SDEVP(d), handle);
}

static int adev_set_audio_port_config(struct audio_hw_device *d,
                                      const struct audio_port_config *config) {
    if (SDEV(d)->set_audio_port_config == NULL) return -ENOSYS;
    struct legacy_port_config legacy;
    port_config_to_legacy(config, &legacy);
    return SDEV(d)->set_audio_port_config(SDEVP(d), &legacy);
}

static int adev_close(hw_device_t *device) {
    struct shim_device *w = (struct shim_device *)device;
    int ret = w->stock->common.close(&w->stock->common);
    free(w);
    return ret;
}

/* ------------------------------------------------------------------------ */
/* Module                                                                    */
/* ------------------------------------------------------------------------ */

static int adev_open(const hw_module_t *module, const char *name, hw_device_t **device) {
    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) return -EINVAL;

    void *handle = dlopen(STOCK_HAL_PATH, RTLD_NOW);
    if (handle == NULL) {
        ALOGE("dlopen %s: %s", STOCK_HAL_PATH, dlerror());
        return -EINVAL;
    }
    struct hw_module_t *stock_module = dlsym(handle, HAL_MODULE_INFO_SYM_AS_STR);
    if (stock_module == NULL) {
        ALOGE("dlsym %s: %s", HAL_MODULE_INFO_SYM_AS_STR, dlerror());
        dlclose(handle);
        return -EINVAL;
    }
    stock_module->dso = handle;

    hw_device_t *stock_common = NULL;
    int ret = stock_module->methods->open(stock_module, name, &stock_common);
    if (ret != 0 || stock_common == NULL) {
        ALOGE("stock open: %d", ret);
        return ret != 0 ? ret : -EINVAL;
    }
    struct legacy_device *s = (struct legacy_device *)stock_common;

    struct shim_device *w = calloc(1, sizeof(*w));
    if (w == NULL) {
        s->common.close(&s->common);
        return -ENOMEM;
    }
    w->stock = s;

    w->dev.common.tag = HARDWARE_DEVICE_TAG;
    /* Nothing newer than 3.0 is provided; 3.2 would make the default
     * implementation use the _v7 entry points, which stay NULL. */
    w->dev.common.version = s->common.version > AUDIO_DEVICE_API_VERSION_3_0
                                    ? AUDIO_DEVICE_API_VERSION_3_0
                                    : s->common.version;
    w->dev.common.module = (struct hw_module_t *)module;
    w->dev.common.close = adev_close;

#define FWD(f) w->dev.f = s->f ? adev_##f : NULL
    FWD(get_supported_devices);
    FWD(init_check);
    FWD(set_voice_volume);
    FWD(set_master_volume);
    FWD(get_master_volume);
    FWD(set_mode);
    FWD(set_mic_mute);
    FWD(get_mic_mute);
    FWD(set_parameters);
    FWD(get_parameters);
    FWD(get_input_buffer_size);
    FWD(open_output_stream);
    FWD(close_output_stream);
    FWD(open_input_stream);
    FWD(close_input_stream);
    FWD(get_microphones);
    FWD(dump);
    FWD(set_master_mute);
    FWD(get_master_mute);
#undef FWD
    /* The default implementation calls these without a NULL check when the
     * device version is 3.0 or newer. */
    w->dev.create_audio_patch = adev_create_audio_patch;
    w->dev.release_audio_patch = adev_release_audio_patch;
    w->dev.set_audio_port_config = adev_set_audio_port_config;
    /* get_audio_port is only used for ports with dynamic profiles (USB, HDMI),
     * which the primary module has none of; struct audio_port changed too. */
    w->dev.get_audio_port = NULL;

    ALOGI("wrapped stock HAL, device version 0x%x (stock 0x%x)", w->dev.common.version,
          s->common.version);
    *device = &w->dev.common;
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
        .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
        .common =
                {
                        .tag = HARDWARE_MODULE_TAG,
                        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
                        .hal_api_version = HARDWARE_HAL_API_VERSION,
                        .id = AUDIO_HARDWARE_MODULE_ID,
                        .name = "Exynos 7885 stock audio HAL wrapper",
                        .author = "Gursu Bilisim",
                        .methods = &hal_module_methods,
                },
};
