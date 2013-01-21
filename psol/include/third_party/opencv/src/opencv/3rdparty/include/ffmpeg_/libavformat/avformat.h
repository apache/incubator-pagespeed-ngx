/*
 * copyright (c) 2001 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVFORMAT_AVFORMAT_H
#define AVFORMAT_AVFORMAT_H


/**
 * Return the LIBAVFORMAT_VERSION_INT constant.
 */
unsigned avformat_version(void);

/**
 * Return the libavformat build-time configuration.
 */
const char *avformat_configuration(void);

/**
 * Return the libavformat license.
 */
const char *avformat_license(void);

#include <time.h>
#include <stdio.h>  /* FILE */
#include "libavcodec/avcodec.h"
#include "libavutil/dict.h"

#include "avio.h"
#include "libavformat/version.h"

struct AVFormatContext;


/**
 * @defgroup metadata_api Public Metadata API
 * @{
 * The metadata API allows libavformat to export metadata tags to a client
 * application using a sequence of key/value pairs. Like all strings in FFmpeg,
 * metadata must be stored as UTF-8 encoded Unicode. Note that metadata
 * exported by demuxers isn't checked to be valid UTF-8 in most cases.
 * Important concepts to keep in mind:
 * -  Keys are unique; there can never be 2 tags with the same key. This is
 *    also meant semantically, i.e., a demuxer should not knowingly produce
 *    several keys that are literally different but semantically identical.
 *    E.g., key=Author5, key=Author6. In this example, all authors must be
 *    placed in the same tag.
 * -  Metadata is flat, not hierarchical; there are no subtags. If you
 *    want to store, e.g., the email address of the child of producer Alice
 *    and actor Bob, that could have key=alice_and_bobs_childs_email_address.
 * -  Several modifiers can be applied to the tag name. This is done by
 *    appending a dash character ('-') and the modifier name in the order
 *    they appear in the list below -- e.g. foo-eng-sort, not foo-sort-eng.
 *    -  language -- a tag whose value is localized for a particular language
 *       is appended with the ISO 639-2/B 3-letter language code.
 *       For example: Author-ger=Michael, Author-eng=Mike
 *       The original/default language is in the unqualified "Author" tag.
 *       A demuxer should set a default if it sets any translated tag.
 *    -  sorting  -- a modified version of a tag that should be used for
 *       sorting will have '-sort' appended. E.g. artist="The Beatles",
 *       artist-sort="Beatles, The".
 *
 * -  Demuxers attempt to export metadata in a generic format, however tags
 *    with no generic equivalents are left as they are stored in the container.
 *    Follows a list of generic tag names:
 *
 @verbatim
 album        -- name of the set this work belongs to
 album_artist -- main creator of the set/album, if different from artist.
                 e.g. "Various Artists" for compilation albums.
 artist       -- main creator of the work
 comment      -- any additional description of the file.
 composer     -- who composed the work, if different from artist.
 copyright    -- name of copyright holder.
 creation_time-- date when the file was created, preferably in ISO 8601.
 date         -- date when the work was created, preferably in ISO 8601.
 disc         -- number of a subset, e.g. disc in a multi-disc collection.
 encoder      -- name/settings of the software/hardware that produced the file.
 encoded_by   -- person/group who created the file.
 filename     -- original name of the file.
 genre        -- <self-evident>.
 language     -- main language in which the work is performed, preferably
                 in ISO 639-2 format. Multiple languages can be specified by
                 separating them with commas.
 performer    -- artist who performed the work, if different from artist.
                 E.g for "Also sprach Zarathustra", artist would be "Richard
                 Strauss" and performer "London Philharmonic Orchestra".
 publisher    -- name of the label/publisher.
 service_name     -- name of the service in broadcasting (channel name).
 service_provider -- name of the service provider in broadcasting.
 title        -- name of the work.
 track        -- number of this work in the set, can be in form current/total.
 variant_bitrate -- the total bitrate of the bitrate variant that the current stream is part of
 @endverbatim
 *
 * Look in the examples section for an application example how to use the Metadata API.
 *
 * @}
 */

#if FF_API_OLD_METADATA2
/**
 * @defgroup old_metadata Old metadata API
 * The following functions are deprecated, use
 * their equivalents from libavutil/dict.h instead.
 * @{
 */

#define AV_METADATA_MATCH_CASE      AV_DICT_MATCH_CASE
#define AV_METADATA_IGNORE_SUFFIX   AV_DICT_IGNORE_SUFFIX
#define AV_METADATA_DONT_STRDUP_KEY AV_DICT_DONT_STRDUP_KEY
#define AV_METADATA_DONT_STRDUP_VAL AV_DICT_DONT_STRDUP_VAL
#define AV_METADATA_DONT_OVERWRITE  AV_DICT_DONT_OVERWRITE

typedef attribute_deprecated AVDictionary AVMetadata;
typedef attribute_deprecated AVDictionaryEntry  AVMetadataTag;

typedef struct AVMetadataConv AVMetadataConv;

/**
 * Get a metadata element with matching key.
 *
 * @param prev Set to the previous matching element to find the next.
 *             If set to NULL the first matching element is returned.
 * @param flags Allows case as well as suffix-insensitive comparisons.
 * @return Found tag or NULL, changing key or value leads to undefined behavior.
 */
attribute_deprecated AVDictionaryEntry *
av_metadata_get(AVDictionary *m, const char *key, const AVDictionaryEntry *prev, int flags);

/**
 * Set the given tag in *pm, overwriting an existing tag.
 *
 * @param pm pointer to a pointer to a metadata struct. If *pm is NULL
 * a metadata struct is allocated and put in *pm.
 * @param key tag key to add to *pm (will be av_strduped depending on flags)
 * @param value tag value to add to *pm (will be av_strduped depending on flags).
 *        Passing a NULL value will cause an existing tag to be deleted.
 * @return >= 0 on success otherwise an error code <0
 */
attribute_deprecated int av_metadata_set2(AVDictionary **pm, const char *key, const char *value, int flags);

/**
 * This function is provided for compatibility reason and currently does nothing.
 */
attribute_deprecated void av_metadata_conv(struct AVFormatContext *ctx, const AVMetadataConv *d_conv,
                                                                        const AVMetadataConv *s_conv);

/**
 * Copy metadata from one AVDictionary struct into another.
 * @param dst pointer to a pointer to a AVDictionary struct. If *dst is NULL,
 *            this function will allocate a struct for you and put it in *dst
 * @param src pointer to source AVDictionary struct
 * @param flags flags to use when setting metadata in *dst
 * @note metadata is read using the AV_DICT_IGNORE_SUFFIX flag
 */
attribute_deprecated void av_metadata_copy(AVDictionary **dst, AVDictionary *src, int flags);

/**
 * Free all the memory allocated for an AVDictionary struct.
 */
attribute_deprecated void av_metadata_free(AVDictionary **m);
/**
 * @}
 */
#endif


/* packet functions */


/**
 * Allocate and read the payload of a packet and initialize its
 * fields with default values.
 *
 * @param pkt packet
 * @param size desired payload size
 * @return >0 (read size) if OK, AVERROR_xxx otherwise
 */
int av_get_packet(AVIOContext *s, AVPacket *pkt, int size);


/**
 * Read data and append it to the current content of the AVPacket.
 * If pkt->size is 0 this is identical to av_get_packet.
 * Note that this uses av_grow_packet and thus involves a realloc
 * which is inefficient. Thus this function should only be used
 * when there is no reasonable way to know (an upper bound of)
 * the final size.
 *
 * @param pkt packet
 * @param size amount of data to read
 * @return >0 (read size) if OK, AVERROR_xxx otherwise, previous data
 *         will not be lost even if an error occurs.
 */
int av_append_packet(AVIOContext *s, AVPacket *pkt, int size);

/*************************************************/
/* fractional numbers for exact pts handling */

/**
 * The exact value of the fractional number is: 'val + num / den'.
 * num is assumed to be 0 <= num < den.
 */
typedef struct AVFrac {
    int64_t val, num, den;
} AVFrac;

/*************************************************/
/* input/output formats */

struct AVCodecTag;

/**
 * This structure contains the data a format has to probe a file.
 */
typedef struct AVProbeData {
    const char *filename;
    unsigned char *buf; /**< Buffer must have AVPROBE_PADDING_SIZE of extra allocated bytes filled with zero. */
    int buf_size;       /**< Size of buf except extra allocated bytes */
} AVProbeData;

#define AVPROBE_SCORE_MAX 100               ///< maximum score, half of that is used for file-extension-based detection
#define AVPROBE_PADDING_SIZE 32             ///< extra allocated bytes at the end of the probe buffer

typedef struct AVFormatParameters {
#if FF_API_FORMAT_PARAMETERS
    attribute_deprecated AVRational time_base;
    attribute_deprecated int sample_rate;
    attribute_deprecated int channels;
    attribute_deprecated int width;
    attribute_deprecated int height;
    attribute_deprecated enum PixelFormat pix_fmt;
    attribute_deprecated int channel; /**< Used to select DV channel. */
    attribute_deprecated const char *standard; /**< deprecated, use demuxer-specific options instead. */
    attribute_deprecated unsigned int mpeg2ts_raw:1;  /**< deprecated, use mpegtsraw demuxer */
    /**< deprecated, use mpegtsraw demuxer-specific options instead */
    attribute_deprecated unsigned int mpeg2ts_compute_pcr:1;
    attribute_deprecated unsigned int initial_pause:1;       /**< Do not begin to play the stream
                                                                  immediately (RTSP only). */
    attribute_deprecated unsigned int prealloced_context:1;
#endif
} AVFormatParameters;

//! Demuxer will use avio_open, no opened file should be provided by the caller.
#define AVFMT_NOFILE        0x0001
#define AVFMT_NEEDNUMBER    0x0002 /**< Needs '%d' in filename. */
#define AVFMT_SHOW_IDS      0x0008 /**< Show format stream IDs numbers. */
#define AVFMT_RAWPICTURE    0x0020 /**< Format wants AVPicture structure for
                                      raw picture data. */
#define AVFMT_GLOBALHEADER  0x0040 /**< Format wants global header. */
#define AVFMT_NOTIMESTAMPS  0x0080 /**< Format does not need / have any timestamps. */
#define AVFMT_GENERIC_INDEX 0x0100 /**< Use generic index building code. */
#define AVFMT_TS_DISCONT    0x0200 /**< Format allows timestamp discontinuities. Note, muxers always require valid (monotone) timestamps */
#define AVFMT_VARIABLE_FPS  0x0400 /**< Format allows variable fps. */
#define AVFMT_NODIMENSIONS  0x0800 /**< Format does not need width/height */
#define AVFMT_NOSTREAMS     0x1000 /**< Format does not require any streams */
#define AVFMT_NOBINSEARCH   0x2000 /**< Format does not allow to fallback to binary search via read_timestamp */
#define AVFMT_NOGENSEARCH   0x4000 /**< Format does not allow to fallback to generic search */
#define AVFMT_TS_NONSTRICT  0x8000 /**< Format does not require strictly
                                          increasing timestamps, but they must
                                          still be monotonic */

typedef struct AVOutputFormat {
    const char *name;
    /**
     * Descriptive name for the format, meant to be more human-readable
     * than name. You should use the NULL_IF_CONFIG_SMALL() macro
     * to define it.
     */
    const char *long_name;
    const char *mime_type;
    const char *extensions; /**< comma-separated filename extensions */
    /**
     * size of private data so that it can be allocated in the wrapper
     */
    int priv_data_size;
    /* output support */
    enum CodecID audio_codec; /**< default audio codec */
    enum CodecID video_codec; /**< default video codec */
    int (*write_header)(struct AVFormatContext *);
    int (*write_packet)(struct AVFormatContext *, AVPacket *pkt);
    int (*write_trailer)(struct AVFormatContext *);
    /**
     * can use flags: AVFMT_NOFILE, AVFMT_NEEDNUMBER, AVFMT_RAWPICTURE,
     * AVFMT_GLOBALHEADER, AVFMT_NOTIMESTAMPS, AVFMT_VARIABLE_FPS,
     * AVFMT_NODIMENSIONS, AVFMT_NOSTREAMS
     */
    int flags;

    void *dummy;

    int (*interleave_packet)(struct AVFormatContext *, AVPacket *out,
                             AVPacket *in, int flush);

    /**
     * List of supported codec_id-codec_tag pairs, ordered by "better
     * choice first". The arrays are all terminated by CODEC_ID_NONE.
     */
    const struct AVCodecTag * const *codec_tag;

    enum CodecID subtitle_codec; /**< default subtitle codec */

#if FF_API_OLD_METADATA2
    const AVMetadataConv *metadata_conv;
#endif

    const AVClass *priv_class; ///< AVClass for the private context

    /* private fields */
    struct AVOutputFormat *next;
} AVOutputFormat;

typedef struct AVInputFormat {
    /**
     * A comma separated list of short names for the format. New names
     * may be appended with a minor bump.
     */
    const char *name;

    /**
     * Descriptive name for the format, meant to be more human-readable
     * than name. You should use the NULL_IF_CONFIG_SMALL() macro
     * to define it.
     */
    const char *long_name;

    /**
     * Size of private data so that it can be allocated in the wrapper.
     */
    int priv_data_size;

    /**
     * Tell if a given file has a chance of being parsed as this format.
     * The buffer provided is guaranteed to be AVPROBE_PADDING_SIZE bytes
     * big so you do not have to check for that unless you need more.
     */
    int (*read_probe)(AVProbeData *);

    /**
     * Read the format header and initialize the AVFormatContext
     * structure. Return 0 if OK. 'ap' if non-NULL contains
     * additional parameters. Only used in raw format right
     * now. 'av_new_stream' should be called to create new streams.
     */
    int (*read_header)(struct AVFormatContext *,
                       AVFormatParameters *ap);

    /**
     * Read one packet and put it in 'pkt'. pts and flags are also
     * set. 'av_new_stream' can be called only if the flag
     * AVFMTCTX_NOHEADER is used and only in the calling thread (not in a
     * background thread).
     * @return 0 on success, < 0 on error.
     *         When returning an error, pkt must not have been allocated
     *         or must be freed before returning
     */
    int (*read_packet)(struct AVFormatContext *, AVPacket *pkt);

    /**
     * Close the stream. The AVFormatContext and AVStreams are not
     * freed by this function
     */
    int (*read_close)(struct AVFormatContext *);

#if FF_API_READ_SEEK
    /**
     * Seek to a given timestamp relative to the frames in
     * stream component stream_index.
     * @param stream_index Must not be -1.
     * @param flags Selects which direction should be preferred if no exact
     *              match is available.
     * @return >= 0 on success (but not necessarily the new offset)
     */
    attribute_deprecated int (*read_seek)(struct AVFormatContext *,
                                          int stream_index, int64_t timestamp, int flags);
#endif
    /**
     * Gets the next timestamp in stream[stream_index].time_base units.
     * @return the timestamp or AV_NOPTS_VALUE if an error occurred
     */
    int64_t (*read_timestamp)(struct AVFormatContext *s, int stream_index,
                              int64_t *pos, int64_t pos_limit);

    /**
     * Can use flags: AVFMT_NOFILE, AVFMT_NEEDNUMBER.
     */
    int flags;

    /**
     * If extensions are defined, then no probe is done. You should
     * usually not use extension format guessing because it is not
     * reliable enough
     */
    const char *extensions;

    /**
     * General purpose read-only value that the format can use.
     */
    int value;

    /**
     * Start/resume playing - only meaningful if using a network-based format
     * (RTSP).
     */
    int (*read_play)(struct AVFormatContext *);

    /**
     * Pause playing - only meaningful if using a network-based format
     * (RTSP).
     */
    int (*read_pause)(struct AVFormatContext *);

    const struct AVCodecTag * const *codec_tag;

    /**
     * Seek to timestamp ts.
     * Seeking will be done so that the point from which all active streams
     * can be presented successfully will be closest to ts and within min/max_ts.
     * Active streams are all streams that have AVStream.discard < AVDISCARD_ALL.
     */
    int (*read_seek2)(struct AVFormatContext *s, int stream_index, int64_t min_ts, int64_t ts, int64_t max_ts, int flags);

#if FF_API_OLD_METADATA2
    const AVMetadataConv *metadata_conv;
#endif

    const AVClass *priv_class; ///< AVClass for the private context

    /* private fields */
    struct AVInputFormat *next;
} AVInputFormat;

enum AVStreamParseType {
    AVSTREAM_PARSE_NONE,
    AVSTREAM_PARSE_FULL,       /**< full parsing and repack */
    AVSTREAM_PARSE_HEADERS,    /**< Only parse headers, do not repack. */
    AVSTREAM_PARSE_TIMESTAMPS, /**< full parsing and interpolation of timestamps for frames not starting on a packet boundary */
    AVSTREAM_PARSE_FULL_ONCE,  /**< full parsing and repack of the first frame only, only implemented for H.264 currently */
};

typedef struct AVIndexEntry {
    int64_t pos;
    int64_t timestamp;
#define AVINDEX_KEYFRAME 0x0001
    int flags:2;
    int size:30; //Yeah, trying to keep the size of this small to reduce memory requirements (it is 24 vs. 32 bytes due to possible 8-byte alignment).
    int min_distance;         /**< Minimum distance between this and the previous keyframe, used to avoid unneeded searching. */
} AVIndexEntry;

#define AV_DISPOSITION_DEFAULT   0x0001
#define AV_DISPOSITION_DUB       0x0002
#define AV_DISPOSITION_ORIGINAL  0x0004
#define AV_DISPOSITION_COMMENT   0x0008
#define AV_DISPOSITION_LYRICS    0x0010
#define AV_DISPOSITION_KARAOKE   0x0020

/**
 * Track should be used during playback by default.
 * Useful for subtitle track that should be displayed
 * even when user did not explicitly ask for subtitles.
 */
#define AV_DISPOSITION_FORCED    0x0040
#define AV_DISPOSITION_HEARING_IMPAIRED  0x0080  /**< stream for hearing impaired audiences */
#define AV_DISPOSITION_VISUAL_IMPAIRED   0x0100  /**< stream for visual impaired audiences */
#define AV_DISPOSITION_CLEAN_EFFECTS     0x0200  /**< stream without voice */

/**
 * Stream structure.
 * New fields can be added to the end with minor version bumps.
 * Removal, reordering and changes to existing fields require a major
 * version bump.
 * sizeof(AVStream) must not be used outside libav*.
 */
typedef struct AVStream {
    int index;    /**< stream index in AVFormatContext */
    int id;       /**< format-specific stream ID */
    AVCodecContext *codec; /**< codec context */
    /**
     * Real base framerate of the stream.
     * This is the lowest framerate with which all timestamps can be
     * represented accurately (it is the least common multiple of all
     * framerates in the stream). Note, this value is just a guess!
     * For example, if the time base is 1/90000 and all frames have either
     * approximately 3600 or 1800 timer ticks, then r_frame_rate will be 50/1.
     */
    AVRational r_frame_rate;
    void *priv_data;

    /* internal data used in av_find_stream_info() */
    int64_t first_dts;

    /**
     * encoding: pts generation when outputting stream
     */
    struct AVFrac pts;

    /**
     * This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * time base should be 1/framerate and timestamp increments should be 1.
     * decoding: set by libavformat
     * encoding: set by libavformat in av_write_header
     */
    AVRational time_base;
    int pts_wrap_bits; /**< number of bits in pts (used for wrapping control) */
    /* ffmpeg.c private use */
    int stream_copy; /**< If set, just copy stream. */
    enum AVDiscard discard; ///< Selects which packets can be discarded at will and do not need to be demuxed.

    //FIXME move stuff to a flags field?
    /**
     * Quality, as it has been removed from AVCodecContext and put in AVVideoFrame.
     * MN: dunno if that is the right place for it
     */
    float quality;

    /**
     * Decoding: pts of the first frame of the stream, in stream time base.
     * Only set this if you are absolutely 100% sure that the value you set
     * it to really is the pts of the first frame.
     * This may be undefined (AV_NOPTS_VALUE).
     * @note The ASF header does NOT contain a correct start_time the ASF
     * demuxer must NOT set this.
     */
    int64_t start_time;

    /**
     * Decoding: duration of the stream, in stream time base.
     * If a source file does not specify a duration, but does specify
     * a bitrate, this value will be estimated from bitrate and file size.
     */
    int64_t duration;

    /* av_read_frame() support */
    enum AVStreamParseType need_parsing;
    struct AVCodecParserContext *parser;

    int64_t cur_dts;
    int last_IP_duration;
    int64_t last_IP_pts;
    /* av_seek_frame() support */
    AVIndexEntry *index_entries; /**< Only used if the format does not
                                    support seeking natively. */
    int nb_index_entries;
    unsigned int index_entries_allocated_size;

    int64_t nb_frames;                 ///< number of frames in this stream if known or 0

    int disposition; /**< AV_DISPOSITION_* bit field */

    AVProbeData probe_data;
#define MAX_REORDER_DELAY 16
    int64_t pts_buffer[MAX_REORDER_DELAY+1];

    /**
     * sample aspect ratio (0 if unknown)
     * - encoding: Set by user.
     * - decoding: Set by libavformat.
     */
    AVRational sample_aspect_ratio;

    AVDictionary *metadata;

    /* Intended mostly for av_read_frame() support. Not supposed to be used by */
    /* external applications; try to use something else if at all possible.    */
    const uint8_t *cur_ptr;
    int cur_len;
    AVPacket cur_pkt;

    // Timestamp generation support:
    /**
     * Timestamp corresponding to the last dts sync point.
     *
     * Initialized when AVCodecParserContext.dts_sync_point >= 0 and
     * a DTS is received from the underlying container. Otherwise set to
     * AV_NOPTS_VALUE by default.
     */
    int64_t reference_dts;

    /**
     * Number of packets to buffer for codec probing
     * NOT PART OF PUBLIC API
     */
#define MAX_PROBE_PACKETS 2500
    int probe_packets;

    /**
     * last packet in packet_buffer for this stream when muxing.
     * used internally, NOT PART OF PUBLIC API, dont read or write from outside of libav*
     */
    struct AVPacketList *last_in_packet_buffer;

    /**
     * Average framerate
     */
    AVRational avg_frame_rate;

    /**
     * Number of frames that have been demuxed during av_find_stream_info()
     */
    int codec_info_nb_frames;

    /**
     * Stream Identifier
     * This is the MPEG-TS stream identifier +1
     * 0 means unknown
     */
    int stream_identifier;

    /**
     * Stream informations used internally by av_find_stream_info()
     */
#define MAX_STD_TIMEBASES (60*12+5)
    struct {
        int64_t last_dts;
        int64_t duration_gcd;
        int duration_count;
        double duration_error[MAX_STD_TIMEBASES];
        int64_t codec_info_duration;
    } *info;

    /**
     * flag to indicate that probing is requested
     * NOT PART OF PUBLIC API
     */
    int request_probe;
} AVStream;

#define AV_PROGRAM_RUNNING 1

/**
 * New fields can be added to the end with minor version bumps.
 * Removal, reordering and changes to existing fields require a major
 * version bump.
 * sizeof(AVProgram) must not be used outside libav*.
 */
typedef struct AVProgram {
    int            id;
    int            flags;
    enum AVDiscard discard;        ///< selects which program to discard and which to feed to the caller
    unsigned int   *stream_index;
    unsigned int   nb_stream_indexes;
    AVDictionary *metadata;

    int program_num;
    int pmt_pid;
    int pcr_pid;
} AVProgram;

#define AVFMTCTX_NOHEADER      0x0001 /**< signal that no header is present
                                         (streams are added dynamically) */

typedef struct AVChapter {
    int id;                 ///< unique ID to identify the chapter
    AVRational time_base;   ///< time base in which the start/end timestamps are specified
    int64_t start, end;     ///< chapter start/end time in time_base units
    AVDictionary *metadata;
} AVChapter;

/**
 * Format I/O context.
 * New fields can be added to the end with minor version bumps.
 * Removal, reordering and changes to existing fields require a major
 * version bump.
 * sizeof(AVFormatContext) must not be used outside libav*.
 */
typedef struct AVFormatContext {
    const AVClass *av_class; /**< Set by avformat_alloc_context. */
    /* Can only be iformat or oformat, not both at the same time. */
    struct AVInputFormat *iformat;
    struct AVOutputFormat *oformat;
    void *priv_data;
    AVIOContext *pb;
    unsigned int nb_streams;
    AVStream **streams;
    char filename[1024]; /**< input or output filename */
    /* stream info */
    int64_t timestamp;

    int ctx_flags; /**< Format-specific flags, see AVFMTCTX_xx */
    /* private data for pts handling (do not modify directly). */
    /**
     * This buffer is only needed when packets were already buffered but
     * not decoded, for example to get the codec parameters in MPEG
     * streams.
     */
    struct AVPacketList *packet_buffer;

    /**
     * Decoding: position of the first frame of the component, in
     * AV_TIME_BASE fractional seconds. NEVER set this value directly:
     * It is deduced from the AVStream values.
     */
    int64_t start_time;

    /**
     * Decoding: duration of the stream, in AV_TIME_BASE fractional
     * seconds. Only set this value if you know none of the individual stream
     * durations and also dont set any of them. This is deduced from the
     * AVStream values if not set.
     */
    int64_t duration;

    /**
     * decoding: total file size, 0 if unknown
     */
    int64_t file_size;

    /**
     * Decoding: total stream bitrate in bit/s, 0 if not
     * available. Never set it directly if the file_size and the
     * duration are known as FFmpeg can compute it automatically.
     */
    int bit_rate;

    /* av_read_frame() support */
    AVStream *cur_st;

    /* av_seek_frame() support */
    int64_t data_offset; /**< offset of the first packet */

    int mux_rate;
    unsigned int packet_size;
    int preload;
    int max_delay;

#define AVFMT_NOOUTPUTLOOP -1
#define AVFMT_INFINITEOUTPUTLOOP 0
    /**
     * number of times to loop output in formats that support it
     */
    int loop_output;

    int flags;
#define AVFMT_FLAG_GENPTS       0x0001 ///< Generate missing pts even if it requires parsing future frames.
#define AVFMT_FLAG_IGNIDX       0x0002 ///< Ignore index.
#define AVFMT_FLAG_NONBLOCK     0x0004 ///< Do not block when reading packets from input.
#define AVFMT_FLAG_IGNDTS       0x0008 ///< Ignore DTS on frames that contain both DTS & PTS
#define AVFMT_FLAG_NOFILLIN     0x0010 ///< Do not infer any values from other values, just return what is stored in the container
#define AVFMT_FLAG_NOPARSE      0x0020 ///< Do not use AVParsers, you also must set AVFMT_FLAG_NOFILLIN as the fillin code works on frames and no parsing -> no frames. Also seeking to frames can not work if parsing to find frame boundaries has been disabled
#if FF_API_FLAG_RTP_HINT
#define AVFMT_FLAG_RTP_HINT     0x0040 ///< Deprecated, use the -movflags rtphint muxer specific AVOption instead
#endif
#define AVFMT_FLAG_CUSTOM_IO    0x0080 ///< The caller has supplied a custom AVIOContext, don't avio_close() it.
#define AVFMT_FLAG_MP4A_LATM    0x8000 ///< Enable RTP MP4A-LATM payload
#define AVFMT_FLAG_SORT_DTS    0x10000 ///< try to interleave outputted packets by dts (using this flag can slow demuxing down)
#define AVFMT_FLAG_PRIV_OPT    0x20000 ///< Enable use of private options by delaying codec open (this could be made default once all code is converted)
#define AVFMT_FLAG_KEEP_SIDE_DATA 0x40000 ///< Dont merge side data but keep it seperate.

    int loop_input;

    /**
     * decoding: size of data to probe; encoding: unused.
     */
    unsigned int probesize;

    /**
     * Maximum time (in AV_TIME_BASE units) during which the input should
     * be analyzed in av_find_stream_info().
     */
    int max_analyze_duration;

    const uint8_t *key;
    int keylen;

    unsigned int nb_programs;
    AVProgram **programs;

    /**
     * Forced video codec_id.
     * Demuxing: Set by user.
     */
    enum CodecID video_codec_id;

    /**
     * Forced audio codec_id.
     * Demuxing: Set by user.
     */
    enum CodecID audio_codec_id;

    /**
     * Forced subtitle codec_id.
     * Demuxing: Set by user.
     */
    enum CodecID subtitle_codec_id;

    /**
     * Maximum amount of memory in bytes to use for the index of each stream.
     * If the index exceeds this size, entries will be discarded as
     * needed to maintain a smaller size. This can lead to slower or less
     * accurate seeking (depends on demuxer).
     * Demuxers for which a full in-memory index is mandatory will ignore
     * this.
     * muxing  : unused
     * demuxing: set by user
     */
    unsigned int max_index_size;

    /**
     * Maximum amount of memory in bytes to use for buffering frames
     * obtained from realtime capture devices.
     */
    unsigned int max_picture_buffer;

    unsigned int nb_chapters;
    AVChapter **chapters;

    /**
     * Flags to enable debugging.
     */
    int debug;
#define FF_FDEBUG_TS        0x0001

    /**
     * Raw packets from the demuxer, prior to parsing and decoding.
     * This buffer is used for buffering packets until the codec can
     * be identified, as parsing cannot be done without knowing the
     * codec.
     */
    struct AVPacketList *raw_packet_buffer;
    struct AVPacketList *raw_packet_buffer_end;

    struct AVPacketList *packet_buffer_end;

    AVDictionary *metadata;

    /**
     * Remaining size available for raw_packet_buffer, in bytes.
     * NOT PART OF PUBLIC API
     */
#define RAW_PACKET_BUFFER_SIZE 2500000
    int raw_packet_buffer_remaining_size;

    /**
     * Start time of the stream in real world time, in microseconds
     * since the unix epoch (00:00 1st January 1970). That is, pts=0
     * in the stream was captured at this real world time.
     * - encoding: Set by user.
     * - decoding: Unused.
     */
    int64_t start_time_realtime;

    /**
     * decoding: number of frames used to probe fps
     */
    int fps_probe_size;

    /**
     * Transport stream id.
     * This will be moved into demuxer private options. Thus no API/ABI compatibility
     */
    int ts_id;
} AVFormatContext;

typedef struct AVPacketList {
    AVPacket pkt;
    struct AVPacketList *next;
} AVPacketList;

/**
 * If f is NULL, returns the first registered input format,
 * if f is non-NULL, returns the next registered input format after f
 * or NULL if f is the last one.
 */
AVInputFormat  *av_iformat_next(AVInputFormat  *f);

/**
 * If f is NULL, returns the first registered output format,
 * if f is non-NULL, returns the next registered output format after f
 * or NULL if f is the last one.
 */
AVOutputFormat *av_oformat_next(AVOutputFormat *f);

#if FF_API_GUESS_IMG2_CODEC
attribute_deprecated enum CodecID av_guess_image2_codec(const char *filename);
#endif

/* XXX: Use automatic init with either ELF sections or C file parser */
/* modules. */

/* utils.c */
void av_register_input_format(AVInputFormat *format);
void av_register_output_format(AVOutputFormat *format);

/**
 * Return the output format in the list of registered output formats
 * which best matches the provided parameters, or return NULL if
 * there is no match.
 *
 * @param short_name if non-NULL checks if short_name matches with the
 * names of the registered formats
 * @param filename if non-NULL checks if filename terminates with the
 * extensions of the registered formats
 * @param mime_type if non-NULL checks if mime_type matches with the
 * MIME type of the registered formats
 */
AVOutputFormat *av_guess_format(const char *short_name,
                                const char *filename,
                                const char *mime_type);

/**
 * Guess the codec ID based upon muxer and filename.
 */
enum CodecID av_guess_codec(AVOutputFormat *fmt, const char *short_name,
                            const char *filename, const char *mime_type,
                            enum AVMediaType type);

/**
 * Send a nice hexadecimal dump of a buffer to the specified file stream.
 *
 * @param f The file stream pointer where the dump should be sent to.
 * @param buf buffer
 * @param size buffer size
 *
 * @see av_hex_dump_log, av_pkt_dump2, av_pkt_dump_log2
 */
void av_hex_dump(FILE *f, uint8_t *buf, int size);

/**
 * Send a nice hexadecimal dump of a buffer to the log.
 *
 * @param avcl A pointer to an arbitrary struct of which the first field is a
 * pointer to an AVClass struct.
 * @param level The importance level of the message, lower values signifying
 * higher importance.
 * @param buf buffer
 * @param size buffer size
 *
 * @see av_hex_dump, av_pkt_dump2, av_pkt_dump_log2
 */
void av_hex_dump_log(void *avcl, int level, uint8_t *buf, int size);

/**
 * Send a nice dump of a packet to the specified file stream.
 *
 * @param f The file stream pointer where the dump should be sent to.
 * @param pkt packet to dump
 * @param dump_payload True if the payload must be displayed, too.
 * @param st AVStream that the packet belongs to
 */
void av_pkt_dump2(FILE *f, AVPacket *pkt, int dump_payload, AVStream *st);


/**
 * Send a nice dump of a packet to the log.
 *
 * @param avcl A pointer to an arbitrary struct of which the first field is a
 * pointer to an AVClass struct.
 * @param level The importance level of the message, lower values signifying
 * higher importance.
 * @param pkt packet to dump
 * @param dump_payload True if the payload must be displayed, too.
 * @param st AVStream that the packet belongs to
 */
void av_pkt_dump_log2(void *avcl, int level, AVPacket *pkt, int dump_payload,
                      AVStream *st);

#if FF_API_PKT_DUMP
attribute_deprecated void av_pkt_dump(FILE *f, AVPacket *pkt, int dump_payload);
attribute_deprecated void av_pkt_dump_log(void *avcl, int level, AVPacket *pkt,
                                          int dump_payload);
#endif

/**
 * Initialize libavformat and register all the muxers, demuxers and
 * protocols. If you do not call this function, then you can select
 * exactly which formats you want to support.
 *
 * @see av_register_input_format()
 * @see av_register_output_format()
 * @see av_register_protocol()
 */
void av_register_all(void);

/**
 * Get the CodecID for the given codec tag tag.
 * If no codec id is found returns CODEC_ID_NONE.
 *
 * @param tags list of supported codec_id-codec_tag pairs, as stored
 * in AVInputFormat.codec_tag and AVOutputFormat.codec_tag
 */
enum CodecID av_codec_get_id(const struct AVCodecTag * const *tags, unsigned int tag);

/**
 * Get the codec tag for the given codec id id.
 * If no codec tag is found returns 0.
 *
 * @param tags list of supported codec_id-codec_tag pairs, as stored
 * in AVInputFormat.codec_tag and AVOutputFormat.codec_tag
 */
unsigned int av_codec_get_tag(const struct AVCodecTag * const *tags, enum CodecID id);

/* media file input */

/**
 * Find AVInputFormat based on the short name of the input format.
 */
AVInputFormat *av_find_input_format(const char *short_name);

/**
 * Guess the file format.
 *
 * @param is_opened Whether the file is already opened; determines whether
 *                  demuxers with or without AVFMT_NOFILE are probed.
 */
AVInputFormat *av_probe_input_format(AVProbeData *pd, int is_opened);

/**
 * Guess the file format.
 *
 * @param is_opened Whether the file is already opened; determines whether
 *                  demuxers with or without AVFMT_NOFILE are probed.
 * @param score_max A probe score larger that this is required to accept a
 *                  detection, the variable is set to the actual detection
 *                  score afterwards.
 *                  If the score is <= AVPROBE_SCORE_MAX / 4 it is recommended
 *                  to retry with a larger probe buffer.
 */
AVInputFormat *av_probe_input_format2(AVProbeData *pd, int is_opened, int *score_max);

/**
 * Guess the file format.
 *
 * @param is_opened Whether the file is already opened; determines whether
 *                  demuxers with or without AVFMT_NOFILE are probed.
 * @param score_ret The score of the best detection.
 */
AVInputFormat *av_probe_input_format3(AVProbeData *pd, int is_opened, int *score_ret);

/**
 * Probe a bytestream to determine the input format. Each time a probe returns
 * with a score that is too low, the probe buffer size is increased and another
 * attempt is made. When the maximum probe size is reached, the input format
 * with the highest score is returned.
 *
 * @param pb the bytestream to probe
 * @param fmt the input format is put here
 * @param filename the filename of the stream
 * @param logctx the log context
 * @param offset the offset within the bytestream to probe from
 * @param max_probe_size the maximum probe buffer size (zero for default)
 * @return 0 in case of success, a negative value corresponding to an
 * AVERROR code otherwise
 */
int av_probe_input_buffer(AVIOContext *pb, AVInputFormat **fmt,
                          const char *filename, void *logctx,
                          unsigned int offset, unsigned int max_probe_size);

#if FF_API_FORMAT_PARAMETERS
/**
 * Allocate all the structures needed to read an input stream.
 *        This does not open the needed codecs for decoding the stream[s].
 * @deprecated use avformat_open_input instead.
 */
attribute_deprecated int av_open_input_stream(AVFormatContext **ic_ptr,
                         AVIOContext *pb, const char *filename,
                         AVInputFormat *fmt, AVFormatParameters *ap);

/**
 * Open a media file as input. The codecs are not opened. Only the file
 * header (if present) is read.
 *
 * @param ic_ptr The opened media file handle is put here.
 * @param filename filename to open
 * @param fmt If non-NULL, force the file format to use.
 * @param buf_size optional buffer size (zero if default is OK)
 * @param ap Additional parameters needed when opening the file
 *           (NULL if default).
 * @return 0 if OK, AVERROR_xxx otherwise
 *
 * @deprecated use avformat_open_input instead.
 */
attribute_deprecated int av_open_input_file(AVFormatContext **ic_ptr, const char *filename,
                       AVInputFormat *fmt,
                       int buf_size,
                       AVFormatParameters *ap);
#endif

/**
 * Open an input stream and read the header. The codecs are not opened.
 * The stream must be closed with av_close_input_file().
 *
 * @param ps Pointer to user-supplied AVFormatContext (allocated by avformat_alloc_context).
 *           May be a pointer to NULL, in which case an AVFormatContext is allocated by this
 *           function and written into ps.
 *           Note that a user-supplied AVFormatContext will be freed on failure.
 * @param filename Name of the stream to open.
 * @param fmt If non-NULL, this parameter forces a specific input format.
 *            Otherwise the format is autodetected.
 * @param options  A dictionary filled with AVFormatContext and demuxer-private options.
 *                 On return this parameter will be destroyed and replaced with a dict containing
 *                 options that were not found. May be NULL.
 *
 * @return 0 on success, a negative AVERROR on failure.
 *
 * @note If you want to use custom IO, preallocate the format context and set its pb field.
 */
int avformat_open_input(AVFormatContext **ps, const char *filename, AVInputFormat *fmt, AVDictionary **options);

int av_demuxer_open(AVFormatContext *ic, AVFormatParameters *ap);

/**
 * Allocate an AVFormatContext.
 * avformat_free_context() can be used to free the context and everything
 * allocated by the framework within it.
 */
AVFormatContext *avformat_alloc_context(void);

#if FF_API_ALLOC_OUTPUT_CONTEXT
/**
 * @deprecated deprecated in favor of avformat_alloc_output_context2()
 */
attribute_deprecated
AVFormatContext *avformat_alloc_output_context(const char *format,
                                               AVOutputFormat *oformat,
                                               const char *filename);
#endif

/**
 * Allocate an AVFormatContext for an output format.
 * avformat_free_context() can be used to free the context and
 * everything allocated by the framework within it.
 *
 * @param *ctx is set to the created format context, or to NULL in
 * case of failure
 * @param oformat format to use for allocating the context, if NULL
 * format_name and filename are used instead
 * @param format_name the name of output format to use for allocating the
 * context, if NULL filename is used instead
 * @param filename the name of the filename to use for allocating the
 * context, may be NULL
 * @return >= 0 in case of success, a negative AVERROR code in case of
 * failure
 */
int avformat_alloc_output_context2(AVFormatContext **ctx, AVOutputFormat *oformat,
                                   const char *format_name, const char *filename);

/**
 * Read packets of a media file to get stream information. This
 * is useful for file formats with no headers such as MPEG. This
 * function also computes the real framerate in case of MPEG-2 repeat
 * frame mode.
 * The logical file position is not changed by this function;
 * examined packets may be buffered for later processing.
 *
 * @param ic media file handle
 * @return >=0 if OK, AVERROR_xxx on error
 * @todo Let the user decide somehow what information is needed so that
 *       we do not waste time getting stuff the user does not need.
 */
int av_find_stream_info(AVFormatContext *ic);

/**
 * Find the "best" stream in the file.
 * The best stream is determined according to various heuristics as the most
 * likely to be what the user expects.
 * If the decoder parameter is non-NULL, av_find_best_stream will find the
 * default decoder for the stream's codec; streams for which no decoder can
 * be found are ignored.
 *
 * @param ic                media file handle
 * @param type              stream type: video, audio, subtitles, etc.
 * @param wanted_stream_nb  user-requested stream number,
 *                          or -1 for automatic selection
 * @param related_stream    try to find a stream related (eg. in the same
 *                          program) to this one, or -1 if none
 * @param decoder_ret       if non-NULL, returns the decoder for the
 *                          selected stream
 * @param flags             flags; none are currently defined
 * @return  the non-negative stream number in case of success,
 *          AVERROR_STREAM_NOT_FOUND if no stream with the requested type
 *          could be found,
 *          AVERROR_DECODER_NOT_FOUND if streams were found but no decoder
 * @note  If av_find_best_stream returns successfully and decoder_ret is not
 *        NULL, then *decoder_ret is guaranteed to be set to a valid AVCodec.
 */
int av_find_best_stream(AVFormatContext *ic,
                        enum AVMediaType type,
                        int wanted_stream_nb,
                        int related_stream,
                        AVCodec **decoder_ret,
                        int flags);

/**
 * Read a transport packet from a media file.
 *
 * This function is obsolete and should never be used.
 * Use av_read_frame() instead.
 *
 * @param s media file handle
 * @param pkt is filled
 * @return 0 if OK, AVERROR_xxx on error
 */
int av_read_packet(AVFormatContext *s, AVPacket *pkt);

/**
 * Return the next frame of a stream.
 * This function returns what is stored in the file, and does not validate
 * that what is there are valid frames for the decoder. It will split what is
 * stored in the file into frames and return one for each call. It will not
 * omit invalid data between valid frames so as to give the decoder the maximum
 * information possible for decoding.
 *
 * The returned packet is valid
 * until the next av_read_frame() or until av_close_input_file() and
 * must be freed with av_free_packet. For video, the packet contains
 * exactly one frame. For audio, it contains an integer number of
 * frames if each frame has a known fixed size (e.g. PCM or ADPCM
 * data). If the audio frames have a variable size (e.g. MPEG audio),
 * then it contains one frame.
 *
 * pkt->pts, pkt->dts and pkt->duration are always set to correct
 * values in AVStream.time_base units (and guessed if the format cannot
 * provide them). pkt->pts can be AV_NOPTS_VALUE if the video format
 * has B-frames, so it is better to rely on pkt->dts if you do not
 * decompress the payload.
 *
 * @return 0 if OK, < 0 on error or end of file
 */
int av_read_frame(AVFormatContext *s, AVPacket *pkt);

/**
 * Seek to the keyframe at timestamp.
 * 'timestamp' in 'stream_index'.
 * @param stream_index If stream_index is (-1), a default
 * stream is selected, and timestamp is automatically converted
 * from AV_TIME_BASE units to the stream specific time_base.
 * @param timestamp Timestamp in AVStream.time_base units
 *        or, if no stream is specified, in AV_TIME_BASE units.
 * @param flags flags which select direction and seeking mode
 * @return >= 0 on success
 */
int av_seek_frame(AVFormatContext *s, int stream_index, int64_t timestamp,
                  int flags);

/**
 * Seek to timestamp ts.
 * Seeking will be done so that the point from which all active streams
 * can be presented successfully will be closest to ts and within min/max_ts.
 * Active streams are all streams that have AVStream.discard < AVDISCARD_ALL.
 *
 * If flags contain AVSEEK_FLAG_BYTE, then all timestamps are in bytes and
 * are the file position (this may not be supported by all demuxers).
 * If flags contain AVSEEK_FLAG_FRAME, then all timestamps are in frames
 * in the stream with stream_index (this may not be supported by all demuxers).
 * Otherwise all timestamps are in units of the stream selected by stream_index
 * or if stream_index is -1, in AV_TIME_BASE units.
 * If flags contain AVSEEK_FLAG_ANY, then non-keyframes are treated as
 * keyframes (this may not be supported by all demuxers).
 *
 * @param stream_index index of the stream which is used as time base reference
 * @param min_ts smallest acceptable timestamp
 * @param ts target timestamp
 * @param max_ts largest acceptable timestamp
 * @param flags flags
 * @return >=0 on success, error code otherwise
 *
 * @note This is part of the new seek API which is still under construction.
 *       Thus do not use this yet. It may change at any time, do not expect
 *       ABI compatibility yet!
 */
int avformat_seek_file(AVFormatContext *s, int stream_index, int64_t min_ts, int64_t ts, int64_t max_ts, int flags);

/**
 * Start playing a network-based stream (e.g. RTSP stream) at the
 * current position.
 */
int av_read_play(AVFormatContext *s);

/**
 * Pause a network-based stream (e.g. RTSP stream).
 *
 * Use av_read_play() to resume it.
 */
int av_read_pause(AVFormatContext *s);

/**
 * Free a AVFormatContext allocated by av_open_input_stream.
 * @param s context to free
 */
void av_close_input_stream(AVFormatContext *s);

/**
 * Close a media file (but not its codecs).
 *
 * @param s media file handle
 */
void av_close_input_file(AVFormatContext *s);

/**
 * Free an AVFormatContext and all its streams.
 * @param s context to free
 */
void avformat_free_context(AVFormatContext *s);

/**
 * Add a new stream to a media file.
 *
 * Can only be called in the read_header() function. If the flag
 * AVFMTCTX_NOHEADER is in the format context, then new streams
 * can be added in read_packet too.
 *
 * @param s media file handle
 * @param id file-format-dependent stream ID
 */
AVStream *av_new_stream(AVFormatContext *s, int id);
AVProgram *av_new_program(AVFormatContext *s, int id);

/**
 * Set the pts for a given stream. If the new values would be invalid
 * (<= 0), it leaves the AVStream unchanged.
 *
 * @param s stream
 * @param pts_wrap_bits number of bits effectively used by the pts
 *        (used for wrap control, 33 is the value for MPEG)
 * @param pts_num numerator to convert to seconds (MPEG: 1)
 * @param pts_den denominator to convert to seconds (MPEG: 90000)
 */
void av_set_pts_info(AVStream *s, int pts_wrap_bits,
                     unsigned int pts_num, unsigned int pts_den);

#define AVSEEK_FLAG_BACKWARD 1 ///< seek backward
#define AVSEEK_FLAG_BYTE     2 ///< seeking based on position in bytes
#define AVSEEK_FLAG_ANY      4 ///< seek to any frame, even non-keyframes
#define AVSEEK_FLAG_FRAME    8 ///< seeking based on frame number

int av_find_default_stream_index(AVFormatContext *s);

/**
 * Get the index for a specific timestamp.
 * @param flags if AVSEEK_FLAG_BACKWARD then the returned index will correspond
 *                 to the timestamp which is <= the requested one, if backward
 *                 is 0, then it will be >=
 *              if AVSEEK_FLAG_ANY seek to any frame, only keyframes otherwise
 * @return < 0 if no such timestamp could be found
 */
int av_index_search_timestamp(AVStream *st, int64_t timestamp, int flags);

/**
 * Add an index entry into a sorted list. Update the entry if the list
 * already contains it.
 *
 * @param timestamp timestamp in the time base of the given stream
 */
int av_add_index_entry(AVStream *st, int64_t pos, int64_t timestamp,
                       int size, int distance, int flags);

/**
 * Perform a binary search using av_index_search_timestamp() and
 * AVInputFormat.read_timestamp().
 * This is not supposed to be called directly by a user application,
 * but by demuxers.
 * @param target_ts target timestamp in the time base of the given stream
 * @param stream_index stream number
 */
int av_seek_frame_binary(AVFormatContext *s, int stream_index,
                         int64_t target_ts, int flags);

/**
 * Update cur_dts of all streams based on the given timestamp and AVStream.
 *
 * Stream ref_st unchanged, others set cur_dts in their native time base.
 * Only needed for timestamp wrapping or if (dts not set and pts!=dts).
 * @param timestamp new dts expressed in time_base of param ref_st
 * @param ref_st reference stream giving time_base of param timestamp
 */
void av_update_cur_dts(AVFormatContext *s, AVStream *ref_st, int64_t timestamp);

/**
 * Perform a binary search using read_timestamp().
 * This is not supposed to be called directly by a user application,
 * but by demuxers.
 * @param target_ts target timestamp in the time base of the given stream
 * @param stream_index stream number
 */
int64_t av_gen_search(AVFormatContext *s, int stream_index,
                      int64_t target_ts, int64_t pos_min,
                      int64_t pos_max, int64_t pos_limit,
                      int64_t ts_min, int64_t ts_max,
                      int flags, int64_t *ts_ret,
                      int64_t (*read_timestamp)(struct AVFormatContext *, int , int64_t *, int64_t ));

/**
 * media file output
 */
#if FF_API_FORMAT_PARAMETERS
/**
 * @deprecated pass the options to avformat_write_header directly.
 */
attribute_deprecated int av_set_parameters(AVFormatContext *s, AVFormatParameters *ap);
#endif

/**
 * Split a URL string into components.
 *
 * The pointers to buffers for storing individual components may be null,
 * in order to ignore that component. Buffers for components not found are
 * set to empty strings. If the port is not found, it is set to a negative
 * value.
 *
 * @param proto the buffer for the protocol
 * @param proto_size the size of the proto buffer
 * @param authorization the buffer for the authorization
 * @param authorization_size the size of the authorization buffer
 * @param hostname the buffer for the host name
 * @param hostname_size the size of the hostname buffer
 * @param port_ptr a pointer to store the port number in
 * @param path the buffer for the path
 * @param path_size the size of the path buffer
 * @param url the URL to split
 */
void av_url_split(char *proto,         int proto_size,
                  char *authorization, int authorization_size,
                  char *hostname,      int hostname_size,
                  int *port_ptr,
                  char *path,          int path_size,
                  const char *url);

/**
 * Allocate the stream private data and write the stream header to
 * an output media file.
 *
 * @param s Media file handle, must be allocated with avformat_alloc_context().
 *          Its oformat field must be set to the desired output format;
 *          Its pb field must be set to an already openened AVIOContext.
 * @param options  An AVDictionary filled with AVFormatContext and muxer-private options.
 *                 On return this parameter will be destroyed and replaced with a dict containing
 *                 options that were not found. May be NULL.
 *
 * @return 0 on success, negative AVERROR on failure.
 *
 * @see av_opt_find, av_dict_set, avio_open, av_oformat_next.
 */
int avformat_write_header(AVFormatContext *s, AVDictionary **options);

#if FF_API_FORMAT_PARAMETERS
/**
 * Allocate the stream private data and write the stream header to an
 * output media file.
 * @note: this sets stream time-bases, if possible to stream->codec->time_base
 * but for some formats it might also be some other time base
 *
 * @param s media file handle
 * @return 0 if OK, AVERROR_xxx on error
 *
 * @deprecated use avformat_write_header.
 */
attribute_deprecated int av_write_header(AVFormatContext *s);
#endif

/**
 * Write a packet to an output media file.
 *
 * The packet shall contain one audio or video frame.
 * The packet must be correctly interleaved according to the container
 * specification, if not then av_interleaved_write_frame must be used.
 *
 * @param s media file handle
 * @param pkt The packet, which contains the stream_index, buf/buf_size,
              dts/pts, ...
 * @return < 0 on error, = 0 if OK, 1 if end of stream wanted
 */
int av_write_frame(AVFormatContext *s, AVPacket *pkt);

/**
 * Write a packet to an output media file ensuring correct interleaving.
 *
 * The packet must contain one audio or video frame.
 * If the packets are already correctly interleaved, the application should
 * call av_write_frame() instead as it is slightly faster. It is also important
 * to keep in mind that completely non-interleaved input will need huge amounts
 * of memory to interleave with this, so it is preferable to interleave at the
 * demuxer level.
 *
 * @param s media file handle
 * @param pkt The packet, which contains the stream_index, buf/buf_size,
              dts/pts, ...
 * @return < 0 on error, = 0 if OK, 1 if end of stream wanted
 */
int av_interleaved_write_frame(AVFormatContext *s, AVPacket *pkt);

/**
 * Interleave a packet per dts in an output media file.
 *
 * Packets with pkt->destruct == av_destruct_packet will be freed inside this
 * function, so they cannot be used after it. Note that calling av_free_packet()
 * on them is still safe.
 *
 * @param s media file handle
 * @param out the interleaved packet will be output here
 * @param pkt the input packet
 * @param flush 1 if no further packets are available as input and all
 *              remaining packets should be output
 * @return 1 if a packet was output, 0 if no packet could be output,
 *         < 0 if an error occurred
 */
int av_interleave_packet_per_dts(AVFormatContext *s, AVPacket *out,
                                 AVPacket *pkt, int flush);

/**
 * Write the stream trailer to an output media file and free the
 * file private data.
 *
 * May only be called after a successful call to av_write_header.
 *
 * @param s media file handle
 * @return 0 if OK, AVERROR_xxx on error
 */
int av_write_trailer(AVFormatContext *s);

#if FF_API_DUMP_FORMAT
/**
 * @deprecated Deprecated in favor of av_dump_format().
 */
attribute_deprecated void dump_format(AVFormatContext *ic,
                                      int index,
                                      const char *url,
                                      int is_output);
#endif

void av_dump_format(AVFormatContext *ic,
                    int index,
                    const char *url,
                    int is_output);

#if FF_API_PARSE_DATE
/**
 * Parse datestr and return a corresponding number of microseconds.
 *
 * @param datestr String representing a date or a duration.
 * See av_parse_time() for the syntax of the provided string.
 * @deprecated in favor of av_parse_time()
 */
attribute_deprecated
int64_t parse_date(const char *datestr, int duration);
#endif

/**
 * Get the current time in microseconds.
 */
int64_t av_gettime(void);

#if FF_API_FIND_INFO_TAG
/**
 * @deprecated use av_find_info_tag in libavutil instead.
 */
attribute_deprecated int find_info_tag(char *arg, int arg_size, const char *tag1, const char *info);
#endif

/**
 * Return in 'buf' the path with '%d' replaced by a number.
 *
 * Also handles the '%0nd' format where 'n' is the total number
 * of digits and '%%'.
 *
 * @param buf destination buffer
 * @param buf_size destination buffer size
 * @param path numbered sequence string
 * @param number frame number
 * @return 0 if OK, -1 on format error
 */
int av_get_frame_filename(char *buf, int buf_size,
                          const char *path, int number);

/**
 * Check whether filename actually is a numbered sequence generator.
 *
 * @param filename possible numbered sequence string
 * @return 1 if a valid numbered sequence string, 0 otherwise
 */
int av_filename_number_test(const char *filename);

/**
 * Generate an SDP for an RTP session.
 *
 * @param ac array of AVFormatContexts describing the RTP streams. If the
 *           array is composed by only one context, such context can contain
 *           multiple AVStreams (one AVStream per RTP stream). Otherwise,
 *           all the contexts in the array (an AVCodecContext per RTP stream)
 *           must contain only one AVStream.
 * @param n_files number of AVCodecContexts contained in ac
 * @param buf buffer where the SDP will be stored (must be allocated by
 *            the caller)
 * @param size the size of the buffer
 * @return 0 if OK, AVERROR_xxx on error
 */
int av_sdp_create(AVFormatContext *ac[], int n_files, char *buf, int size);

#if FF_API_SDP_CREATE
attribute_deprecated int avf_sdp_create(AVFormatContext *ac[], int n_files, char *buff, int size);
#endif

/**
 * Return a positive value if the given filename has one of the given
 * extensions, 0 otherwise.
 *
 * @param extensions a comma-separated list of filename extensions
 */
int av_match_ext(const char *filename, const char *extensions);

#endif /* AVFORMAT_AVFORMAT_H */
