#ifndef _MOD_TS_FILE_H_
#define _MOD_TS_FILE_H_

#include <iostream>
#include <fstream>
#include <vector>
#include <string.h>
#include <thread>
#include <mutex>
#include <cstring>
#include <utility>
#include <cstdint>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include "parse_json.h"

#define STREAMTYPE_11172_VIDEO 0x01
#define STREAMTYPE_13818_VIDEO 0x02
#define STREAMTYPE_11172_AUDIO 0x03
#define STREAMTYPE_13818_AUDIO 0x04
#define STREAMTYPE_13818_PRIVATE 0x05
#define STREAMTYPE_13818_PES_PRIVATE 0x06
#define STREAMTYPE_13522_MHPEG 0x07
#define STREAMTYPE_13818_DSMCC 0x08
#define STREAMTYPE_ITU_222_1 0x09
#define STREAMTYPE_13818_A 0x0a
#define STREAMTYPE_13818_B 0x0b
#define STREAMTYPE_13818_C 0x0c
#define STREAMTYPE_13818_D 0x0d
#define STREAMTYPE_13818_AUX 0x0e
#define STREAMTYPE_AAC_AUDIO 0x0f
#define STREAMTYPE_MPEG4_AUDIO 0x11
#define STREAMTYPE_H264_VIDEO 0x1b
#define STREAMTYPE_AVS_VIDEO 0x42
#define STREAMTYPE_AC3_AUDIO 0x81
#define STREAMTYPE_DTS_AUDIO 0x82
#define TS_PACHET_LENGTH 188

#define STREAM_ID_PROGRAM_STREAM_MAP  0xbc
#define STREAM_ID_PRIVATE_STREAM_1    0xbd
#define STREAM_ID_PADDING_STREAM      0xbe
#define STREAM_ID_PRIVATE_STREAM_2    0xbf
#define STREAM_ID_ECM_STREAM          0xf0
#define STREAM_ID_EMM_STREAM          0xf1
#define STREAM_ID_DSMCC_STREAM        0xf2
#define STREAM_ID_13522_STREAM        0xf3
#define STREAM_ID_H222_A_STREAM       0xf4
#define STREAM_ID_H222_B_STREAM       0xf5
#define STREAM_ID_H222_C_STREAM       0xf6
#define STREAM_ID_H222_D_STREAM       0xf7
#define STREAM_ID_H222_E_STREAM       0xf8
#define STREAM_ID_ANCILLARY_STREAM    0xf9
#define STREAM_ID_PROGRAM_STREAM_DIRECTORY  0xff

/*------------------------------- Base Struct -------------------------------*/
typedef struct TsPacketHeader
{
    unsigned sync_byte                      : 8; //同步字节, 固定0x47,表示后面的是一个TS分组
    unsigned transport_error_indicator      : 1; //传输误码指示
    unsigned payload_unit_start_indicator   : 1; //有效荷载单元起始指示 
    unsigned transport_priority             : 1; //传输优先, 1表示高优先级,传输机制可能用到，解码用不着
    unsigned pid                            : 13; //PID, PAT_pid = 0
    unsigned transport_scrambling_control   : 2; //传输加扰控制 
    unsigned adaption_field_control         : 2; //自适应控制: 01仅含有效负载, 10仅含调整字段, 11含有调整字段和有效负载。00解码器不进行处理
    unsigned continuity_counter             : 4; //连续计数，4 bits，range:0-15
public:
    static TsPacketHeader* parseTsPacketHeader(u_char* buffer, int buffer_length);
    static void writeTsPacketHeaderToChar(const TsPacketHeader* header, char* buffer);
} TsPacketHeader;    //32 bits, 4 bytes

typedef struct Program{
    unsigned program_number                 :16;    // 节目号
    unsigned reserved                       :3; 
    unsigned network_program_map_PID        :13;    // 节目映射表的pid
}Program;

typedef struct TsPAT{
    unsigned table_id                       :8;    // 8 bit
    unsigned section_syntax_indicator       :1;    // 1 bit 
    unsigned zero                           :1;    // 1 bit
    unsigned reserved_1                     :2;
    unsigned section_length                 :12;
    unsigned transport_stream_id            :16;  
    unsigned reserved_2                     :2;  
    unsigned version_number                 :5;
    unsigned current_next_indicator         :1;
    unsigned section_number                 :8;
    unsigned last_section_number            :8;
    std::vector<Program> program;           // 4 byte
    unsigned reserved_3                     :3;
    unsigned network_PID                    :13;
    unsigned CRC_32                         :32;
public:
    static TsPAT* parsePAT(u_char* buffer, int buffer_length);
}TsPAT;

typedef struct TsPmtStream
{
    unsigned stream_type     : 8; // 指示特定PID的节目元素包的类型。该处PID由elementary PID指定
    unsigned elementary_PID  : 13; // 该域指示TS包的PID值。这些TS包含有相关的节目元素
    unsigned ES_info_length  : 12; // 前两位bit为00。该域指示跟随其后的描述相关节目元素的byte数
    unsigned descriptor;
} TS_PMT_Stream;

typedef struct TsPMT
{
    unsigned table_id                       : 8; // 固定为0x02，表示PMT表
    unsigned section_syntax_indicator       : 1; // 固定为0x01
    unsigned zero                           : 1; // 0x00
    unsigned reserved_1                     : 2; // 0x03
    unsigned section_length                 : 12; // 首先两位bit置为00，它指示段的byte数，由段长度域开始，包含CRC
    unsigned program_number                 : 16; // 指出该节目对应于可应用的Program map PID
    unsigned reserved_2                     : 2; // 0x03
    unsigned version_number                 : 5; // 指出TS流中Program map section的版本号
    unsigned current_next_indicator         : 1; // 当该位置1时，当前传送的Program map section可用
    // 当该位置0时，指示当前传送的Program map section不可用，下一个TS流的Program map section有效
    unsigned section_number                 : 8; // 固定为0x00
    unsigned last_section_number            : 8; // 固定为0x00
    unsigned reserved_3                     : 3; // 0x07
    unsigned PCR_PID                        : 13; // 指明TS包的PID值，该TS包含有PCR域，
    // 该PCR值对应于由节目号指定的对应节目，如果对于私有数据流的节目定义与PCR无关，这个域的值将为0x1FFF。
    unsigned reserved_4                     : 4; // 预留为0x0F
    unsigned program_info_length            : 12; // 前两位bit为00。该域指出跟随其后对节目信息的描述的byte数。

    std::vector<TsPmtStream> PMT_Stream; // 每个元素包含8位，指示特定PID的节目元素包的类型。该处PID由elementary PID指定
    unsigned reserved_5                     : 3; // 0x07
    unsigned reserved_6                     : 4; // 0x0F
    unsigned CRC_32                         : 32;
public:
    static TsPMT* parsePMT(u_char* buffer, int buffer_length);
}TsPMT;

typedef struct PesPacketHeader{
    u_char start_code_prefix[3];        // 0x000001
    u_char stream_id[1];                // Audio streams (0xC0-0xDF), Video streams (0xE0-0xEF)
    u_char pes_packet_length[2];        // 0 means any length
}PesPacketHeader;   // 6 bytes

typedef struct OptionalPesHeader
{
    unsigned marker_bits                    : 2;    // 10 binary or 0x2 hex
    unsigned scrambling_control             : 2;    // 00 implies not scrambled
    unsigned priority                       : 1;
    unsigned data_alignment_indicator       : 1;    // 1 indicates that the PES packet header is immediately followed by the video start code or audio syncword
    unsigned copyright                      : 1;    // 1 implies copyrighted
    unsigned original_or_copy               : 1;    // 1 implies original
    unsigned pts_dts_indicator              : 2;    // 11 = both present, 01 is forbidden, 10 = only PTS, 00 = no PTS or DTS
    unsigned escr_flag                      : 1; 
    unsigned escr_rate_flag                 : 1;
    unsigned dsm_trick_mode_flag            : 1;
    unsigned additional_copy_info_flag      : 1;
    unsigned crc_flag                       : 1;
    unsigned extension_flag                 : 1;
    unsigned pes_header_length              : 8;
public:
    static OptionalPesHeader* parseOptionalPesHeader(u_char* buffer, int buffer_length);
} OptionalPesHeader;    //24 bits, 3 bytes

typedef enum ModeTsFileStatus{
    Running,
    Stop,
    Err
}ModeTsFileStatus;

/*------------------------------- Working Func -------------------------------*/
class ModTsFile{
public:
    // 给删除任�? 初始化用
    ModTsFile(TaskParam* task_param);

    ~ModTsFile();
    bool Start();
    void Stop();
    bool Success();

private:
    // 重新整理传入的param列表
    bool arrangeParamList();
    bool clearParamList();

    // 线程相关
    void doParsing();
    bool threadInit();
    void threadDeinit();
    ModeTsFileStatus    threadRunning();
    std::mutex          thread_mut;

    /*----------------------------解析------------------------------*/    
    // 找到就返回pts/dts的相对位置的position_pair，注意是相对位置，从buffer的起始位置开始算
    std::pair<unsigned, unsigned> findPtsfromPes(u_char* buffer, int buffer_length);

    /*----------------------------时间相关------------------------------*/
    // 从两个u int 64中获得一个时间差，也就是将△pts转换为时间△time
    double getTimeDiff(u_int64_t start_pts, u_int64_t end_pts);
    // 将所有的pattern叠加，获得当前时间节点的pts修改量
    int64_t getPtsChange(Media media_type, u_int64_t cur_pts);
    // 返回-1，代表删除，返回0，代表不变，返回1，代表重复
    int shouldDeleteThisPack(Media media_type, int pid); 
    // 查找此处是否需要填充空包
    bool shouldFillWithNull(Media media_type);
    // 修改PTS
    bool changePTS(u_char* buffer, int buffer_length, int64_t pts_change);
    // 修改DTS
    bool changeDTS(u_char* buffer, int buffer_length, int64_t pts_change);
    // 从buffer生成u_int64_t的pts`
    u_int64_t combinePts(char buffer[5]);
    // 从u_int64_t的pts写入buffer[5]
    void rewritePts(char* payload, u_int64_t pts);
    // 从u_int64_t的dts写入buffer[5]
    void rewriteDts(char* payload, u_int64_t pts);
    void writeNullPack(char* payload, int buffer_length);

    /*----------------------------辅助------------------------------*/
    // 文件相关
    std::string removeSuffix(const std::string& str, const std::string& suffix);
    bool changeOutoutFile(std::ofstream* file_out, std::string& file_name, int slice_count);

    // pmt表相关
    // 从新的pat中更新pmt列表，删除当前不再存在的pmt结构
    void refreshPmtPidList();
    bool hasPmtNodeFromList(const int pid);
    bool refreshPmtNodeIntoList(const int pid, TsPMT* pmt);
    bool isPmtPacket(const int pid);
    bool isAudioPacket(const int pid);
    bool isVideoPacket(const int pid);
    bool isVideoStream(uint8_t streamType);
    bool isAudioStream(uint8_t streamType);

private:
    // 初始化传递进来的参数
    TaskParam* task_param_;

    // 按类型分类参数
    FuncPattern* cut_param_;
    std::vector<FuncPattern*> pts_pattern_list_;        // pts跳变的参数
    std::vector<FuncPattern*> loss_repeat_list_;        // 重复/丢包参数列表
    std::vector<FuncPattern*> ts_err_list_;             // ts层文件数据变更

    // 文件相关
    std::fstream file_in_;
    std::ofstream file_out_;

    // buffer
    u_char buffer[TS_PACHET_LENGTH];

    // 启动相关
    bool be_started_;
    bool be_success_;

    // 线程相关
    bool thread_be_running_;
    std::thread parsing_thread_;
    ModeTsFileStatus statu_;

    // 时间相关
    u_int64_t start_pts_;
    u_int64_t cur_pts_;
    double cur_time_;
    double start_time_;
    double end_time_;
    
    // ts文件的各种相关参数
    TsPAT* pat_;
    std::vector<std::pair<int, TsPMT*>> pmt_pair_list_; // 随时会被更新

    // 统计信息
    long long program_pack_cnt = 0;
    long long repeated_pack_cnt = 0;
    long long lossed_pack_cnt = 0;
};

/*------------------------------- Debugging Func -------------------------------*/
void printBinaryDigits(unsigned char byte);

#endif  //_MOD_TS_FILE_H_