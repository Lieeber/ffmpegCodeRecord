#include <iostream>
#include <stdlib.h>
#include <string.h>

/**
 * 音频或视频中提取pcm数据。
 */
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio


int main(int argc, char *argv[]) {
    AVFormatContext *pFormatCtx = nullptr;
    int i, audioStream;
    AVCodecContext *pCodecCtx = nullptr;
    AVCodec *pCodec = nullptr;
    AVPacket *packet = nullptr;
    uint8_t *out_buffer = nullptr;
    AVFrame *pFrame = nullptr;
    int ret;
    uint32_t len = 0;
    int got_picture;
    int index = 0;
    int64_t in_channel_layout;
    struct SwrContext *au_convert_ctx = nullptr;

    FILE *pFile = nullptr;
    char url[] = "test.mp4";

    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();
    //Open
    if (avformat_open_input(&pFormatCtx, url, nullptr, nullptr) != 0) {
        printf("Couldn't open input stream.\n");
        return -1;
    }
    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, nullptr) < 0) {
        printf("Couldn't find stream information.\n");
        return -1;
    }
    // Dump valid information onto standard error
    av_dump_format(pFormatCtx, 0, url, false);

    // Find the first audio stream
    audioStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream = i;
            break;
        }

    if (audioStream == -1) {
        printf("Didn't find a audio stream.\n");
        return -1;
    }

    // Get a pointer to the codec context for the audio stream
    pCodecCtx = pFormatCtx->streams[audioStream]->codec;

    // Find the decoder for the audio stream
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        printf("Codec not found.\n");
        return -1;
    }

    // Open codec
    if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }
    pFile = fopen("output.pcm", "wb");
    packet = av_packet_alloc();
    av_init_packet(packet);
    //Out Audio Param
    //nb_samples: AAC-1024 MP3-1152
    AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    //Out Buffer Size
    int out_buffer_size = av_samples_get_buffer_size(nullptr, pCodecCtx->channels, pCodecCtx->frame_size, out_sample_fmt, 1);

    out_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
    pFrame = av_frame_alloc();

    //FIX:Some Codec's Context Information is missing
    in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
    //Swr
    au_convert_ctx = swr_alloc();
    au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, AV_CH_LAYOUT_STEREO, out_sample_fmt, pCodecCtx->sample_rate, in_channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, nullptr);
    swr_init(au_convert_ctx);
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == audioStream) {
            ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_picture, packet);
            if (ret < 0) {
                printf("Error in decoding audio frame.\n");
                return -1;
            }
            if (got_picture > 0) {
                swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t **) pFrame->data, pFrame->nb_samples);
                printf("index:%5d\t pts:%lld\t packet size:%d\n", index, packet->pts, packet->size);

                //Write PCM
                fwrite(out_buffer, 1, out_buffer_size, pFile);
                index++;
            }
        }
        av_free_packet(packet);
    }

    swr_free(&au_convert_ctx);
    fclose(pFile);
    av_free(out_buffer);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return 0;
}