#include "mod_ts_file.h"
#include <iostream>
#include "common/Log.h"

static void writeThisBufferIntoFile(const char* buffer, int buffer_length){
    static bool marker = false;
    static std::ofstream out_file;

    static int cnt = 0;
    std::cout << "Call this func for " << cnt++ << std::endl;

    // 第一次开启文件
    if(!marker){
        std::string debug_file_name = "err_file.ts"; 
        out_file.open(debug_file_name.c_str(), std::ios::app);
        marker = true;
    }

    out_file.write(buffer, buffer_length);
    return;
}

bool verifyHighFourBits(const u_char byte, const u_char mask) {
    // 0010的二进制表示为0x2
    u_char highFourBits = byte >> 4;
    return (highFourBits & 0xF) == mask;
}

/*------------------------------- Base Struct -------------------------------*/
TsPAT* TsPAT::parsePAT(u_char* buffer, int buffer_length) {
    if(buffer == nullptr){
        return nullptr;
    }

    TsPAT* packet = new TsPAT();
    
    packet->table_id                    = buffer[0];
    packet->section_syntax_indicator    = buffer[1] >> 7;
    packet->zero                        = (buffer[1] >> 6) & 0x1;

    // PAT表的table id = 0
    if(packet->table_id != 0 || packet->section_syntax_indicator != 1 || packet->zero != 0){
        delete packet;
        return nullptr;
    }

    packet->reserved_1                  = (buffer[1] >> 4) & 0x3;
    packet->section_length              = ((buffer[1] & 0x0F) << 8) | buffer[2];
    packet->transport_stream_id         = (buffer[3] << 8) | buffer[4];
    packet->reserved_2                  = buffer[5] >> 6;
    packet->version_number              = (buffer[5] >> 1) & 0x1F;
    packet->current_next_indicator      = (buffer[5] << 7) >> 7;
    packet->section_number              = buffer[6];
    packet->last_section_number         = buffer[7];
    
    int len = 3 + packet->section_length;
    packet->CRC_32 = (buffer[len - 4] & 0xFF) << 24 |
                     (buffer[len - 3] & 0xFF) << 16 |
                     (buffer[len - 2] & 0xFF) << 8 |
                     (buffer[len - 1] & 0xFF);
    
    int n = 0;
    for (n = 0; n < packet->section_length - 12; n += 4) {
        unsigned program_num = (buffer[8 + n] << 8) | buffer[9 + n];
        packet->reserved_3 = buffer[10 + n] >> 5;
        
        packet->network_PID = 0x00;
        if (program_num == 0x00) {
            packet->network_PID = ((buffer[10 + n] & 0x1F) << 8) | buffer[11 + n];
        } else {
            Program PAT_program;
            PAT_program.program_number = program_num;
            PAT_program.reserved = buffer[10 + n] >> 5;
            PAT_program.network_program_map_PID = ((buffer[10 + n] & 0x1F) << 8) | buffer[11 + n];
            packet->program.push_back(PAT_program);
        }
    }
    return packet;
}

TsPMT* TsPMT::parsePMT(u_char* buffer, int buffer_length)
{
    if(buffer == nullptr){
        std::cout << "ERROR: Parse PMT: null buffer or out of buffer length" << std::endl;
        return nullptr;
    }

    TsPMT* packet = new TsPMT;

    packet->table_id = buffer[0];
    if(packet->table_id != 0x02){
        delete packet;
        return nullptr;
    }
    packet->section_syntax_indicator = buffer[1] >> 7;
    packet->zero = (buffer[1] >> 6) & 0x01;
    packet->reserved_1 = (buffer[1] >> 4) & 0x03;
    packet->section_length = ((buffer[1] & 0x0F) << 8) | buffer[2];
    packet->program_number = (buffer[3] << 8) | buffer[4];
    packet->reserved_2 = buffer[5] >> 6;
    packet->version_number = (buffer[5] >> 1) & 0x1F;
    packet->current_next_indicator = (buffer[5] << 7) >> 7;
    packet->section_number = buffer[6];
    packet->last_section_number = buffer[7];
    packet->reserved_3 = buffer[8] >> 5;
    packet->PCR_PID = ((buffer[8] & 0x1F) << 8) | buffer[9];

    // Store PCR_PID value in a variable
    unsigned int PCRID = packet->PCR_PID;

    packet->reserved_4 = buffer[10] >> 4;
    packet->program_info_length = ((buffer[10] & 0x0F) << 8) | buffer[11];

    // Get CRC_32
    int len = packet->section_length + 3;
    packet->CRC_32 = (buffer[len - 4] << 24) |
                     (buffer[len - 3] << 16) |
                     (buffer[len - 2] << 8) |
                     buffer[len - 1];

    int pos = 12;

    // Program info descriptor
    if (packet->program_info_length != 0)
        pos += packet->program_info_length;

    // Get stream type and PID
    while (pos <= packet->section_length + 2 - 4)
    {
        TsPmtStream pmt_stream;
        pmt_stream.stream_type = buffer[pos];
        packet->reserved_5 = buffer[pos + 1] >> 5;
        pmt_stream.elementary_PID = ((buffer[pos + 1] & 0x1F) << 8) | buffer[pos + 2];
        packet->reserved_6 = buffer[pos + 3] >> 4;
        pmt_stream.ES_info_length = ((buffer[pos + 3] & 0x0F) << 8) | buffer[pos + 4];

        pmt_stream.descriptor = 0x00;
        if (pmt_stream.ES_info_length != 0)
        {
            pmt_stream.descriptor = buffer[pos + 5];

            for (int len = 2; len <= pmt_stream.ES_info_length; len++)
            {
                pmt_stream.descriptor = (pmt_stream.descriptor << 8) | buffer[pos + 4 + len];
            }
            pos += pmt_stream.ES_info_length;
        }
        pos += 5;
        packet->PMT_Stream.push_back(pmt_stream);
    }

    /*
    std::cout << "Get a pmt, which has " << packet->PMT_Stream.size() << " stream" << std::endl;
    for(auto stream : packet->PMT_Stream){
        std::cout << "stream " << stream.elementary_PID << std::endl;
    }
    */

    return packet;
}

TsPacketHeader* TsPacketHeader::parseTsPacketHeader(u_char* buffer, int buffer_length) {
    if(buffer == nullptr || buffer_length < 4){
        std::cout << "ERROR: Null buffer or Invalid buffer length" << std::endl;
        return nullptr;
    }

    TsPacketHeader* header = new TsPacketHeader;

    header->sync_byte = buffer[0];
    header->transport_error_indicator = (buffer[1] >> 7) & 0x01;
    header->payload_unit_start_indicator = (buffer[1] >> 6) & 0x01;
    header->transport_priority = (buffer[1] >> 5) & 0x01;
    header->pid = ((buffer[1] & 0x1F) << 8) | buffer[2];
    header->transport_scrambling_control = (buffer[3] >> 6) & 0x03;
    header->adaption_field_control = (buffer[3] >> 4) & 0x03;
    header->continuity_counter = buffer[3] & 0x0F;

    return header;
}

void TsPacketHeader::writeTsPacketHeaderToChar(const TsPacketHeader* header, char* buffer){
    buffer[0] = (char)(header->sync_byte & 0xFF);
    buffer[1] = (char)(
        ((header->transport_error_indicator & 0x1) << 7) |
        ((header->payload_unit_start_indicator & 0x1) << 6) |
        ((header->transport_priority & 0x1) << 5) |
        ((header->pid >> 8) & 0x1F)
    );
    buffer[2] = (char)(header->pid & 0xFF);
    buffer[3] = (char)(
        ((header->transport_scrambling_control & 0x3) << 6) |
        ((header->adaption_field_control & 0x3) << 4) |
        (header->continuity_counter & 0xF)
    );
}

OptionalPesHeader* OptionalPesHeader::parseOptionalPesHeader(u_char* buffer, int buffer_length) {
    // 检查输入缓冲区长度是否足够
    if(buffer == nullptr ||  buffer_length < 3){
        std::cout << "ERROR: Null buffer or Optional Pes header out of buffer length" << std::endl;
        return nullptr;
    }

    // 创建OptionalPesHeader结构体对象
    OptionalPesHeader* header = new OptionalPesHeader;

    // 从缓冲区中读取字节并映射到结构体的字段中
    u_char byte = buffer[0];
    header->marker_bits = (byte >> 6) & 0x03;
    header->scrambling_control = (byte >> 4) & 0x03;
    header->priority = (byte >> 3) & 0x01;
    header->data_alignment_indicator = (byte >> 2) & 0x01;
    header->copyright = (byte >> 1) & 0x01;
    header->original_or_copy = byte & 0x01;

    byte = buffer[1];
    header->pts_dts_indicator = (byte >> 6) & 0x03;
    header->escr_flag = (byte >> 5) & 0x01;
    header->escr_rate_flag = (byte >> 4) & 0x01;
    header->dsm_trick_mode_flag = (byte >> 3) & 0x01;
    header->additional_copy_info_flag = (byte >> 2) & 0x01;
    header->crc_flag = (byte >> 1) & 0x01;
    header->extension_flag = byte & 0x01;

    header->pes_header_length = buffer[2];

    return header;
}

/*------------------------------- Working Func -------------------------------*/
// 构造函�?
ModTsFile::ModTsFile(TaskParam* task_param){
    // 获取所有参数
    task_param_ = task_param;

    be_started_ = false;
    be_success_ = false;

    // 线程相关
    thread_be_running_ = false;
    statu_ = ModeTsFileStatus::Stop;

    // 时间相关，当前pts
    start_pts_ = 0;
    cur_pts_ = 0;

    start_time_ = -1;
    end_time_ = -1;

    // ts文件的参数
    pat_ = nullptr;
    pmt_pair_list_.clear();

    // 重新整理传入的task_param列表
    arrangeParamList();
};

// 析构函数
ModTsFile::~ModTsFile(){
    Stop();
    clearParamList();
};

// 启动任务
bool ModTsFile::Start(){
    if(be_started_){
        std::cerr << "TaskParam starting..." << std::endl;
        return false;
    }
    
    // 开启输入文�?
    file_in_.open(task_param_->input_file.c_str(), std::ios_base::in | std::ios_base::out);
    if(!file_in_.is_open()){
        std::cerr << "Failed to open the input file: " << task_param_->input_file << std::endl;
        return false;
    }

    // 开启输出文�?
    file_out_.open(task_param_->output_file.c_str(), std::ios_base::out);
    if(!file_out_.is_open()){
        std::cerr << "Failed to open the output file: " << task_param_->output_file << std::endl;
        file_in_.close();
        return false;
    } 


    // 生成随机数种�?
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    // 启动线程
    thread_be_running_ = true;
    parsing_thread_ = std::thread(&ModTsFile::doParsing, this);

    be_started_ = true;
    std::cout << "Success to start a thread " << std::endl;
    return true;
};

// 停止任务
void ModTsFile::Stop(){
    // 如果没有开启，输出错误
    if(be_started_ == false){
        Log::debug(__FILE__, __LINE__, "Being Stopped, don't stop again");
        return;
    }

    // 关闭线程
    Log::debug(__FILE__, __LINE__, "Trying to stop the thread");
    if(parsing_thread_.joinable()){
        thread_be_running_ = false;
        parsing_thread_.join();
    }

    // 关闭输出文件
    if(file_out_.is_open()){
        file_out_.close();
    }

    // 关闭输入文件
    if(file_in_.is_open()){
        file_in_.close();
    }

    be_started_ = false;
    Log::debug(__FILE__, __LINE__, "Success to stop it");
    return;
};

// 重新整理paramlist 到各自的list中
bool ModTsFile::arrangeParamList(){
    if(task_param_ == nullptr)
        return false;
    // cut_param_:          cut
    // pts_pattern_list_：  add、minus、sin、pulse\mult、div
    // loss_repeat_list_：  loss、repeate
    // ts_err_list_：       others
    for(auto func : task_param_->func_pattern){
        switch (func->pts_func)
        {
        case PtsFunc::cut:
            cut_param_ = func; 
            start_time_ = cut_param_->start_sec;
            end_time_ = cut_param_->end_sec;
            break;
        case PtsFunc::add:
        case PtsFunc::minus:
        case PtsFunc::sin:
        case PtsFunc::pulse:
        case PtsFunc::mult:
            pts_pattern_list_.push_back(func);
            break;
        case PtsFunc::repeate:
        case PtsFunc::pat_delete:
        case PtsFunc::pmt_delete:
        case PtsFunc::loss:
            loss_repeat_list_.push_back(func);
            break;
        case PtsFunc::null:
            ts_err_list_.push_back(func);
        case PtsFunc::others:
            break;
        default:
            break;
        }
    }
    return true;
}

// 清理list的逻辑，因为有的时候会共用指针，所以最终释放需要用这个统一释放
bool ModTsFile::clearParamList(){
    if(cut_param_ != nullptr){
        delete cut_param_;
        cut_param_ = nullptr;
    }

    for(auto param : pts_pattern_list_){
        if(param != nullptr){
            delete param;
            param = nullptr;
        }
    }
    pts_pattern_list_.clear();

    for(auto param : loss_repeat_list_){
        if(param != nullptr){
            delete param;
            param = nullptr;
        }
    }
    loss_repeat_list_.clear();

    for(auto param : ts_err_list_){
        if(param != nullptr){
            delete param;
            param = nullptr;
        }
    }
    ts_err_list_.clear();

    for(auto param : task_param_->func_pattern){
        if(param != nullptr){
            delete param;
            param = nullptr;
        }
    }
    ts_err_list_.clear();
}

// 解析线程
void ModTsFile::doParsing(){
    // 初始化
    thread_be_running_ = true;
    if(!threadInit()){
        thread_be_running_ = false;
        std::cerr << "Failed to init the parsing thread" << std::endl;
    }

    // 线程运行
    statu_ = ModeTsFileStatus::Running;
    while(statu_ != ModeTsFileStatus::Stop){
        std::lock_guard<std::mutex> locker(thread_mut);
        switch (statu_)
        {
        case ModeTsFileStatus::Running:
            statu_ = threadRunning();
            break;
        case ModeTsFileStatus::Err:
            statu_ == ModeTsFileStatus::Stop;
            break;
        default:
            statu_ == ModeTsFileStatus::Stop;
            break;
        }
    };


    // 清理数据
    threadDeinit();
    be_success_ = true;
    thread_be_running_ = false;
    Log::debug(__FILE__, __LINE__, "Exit the parsing thread");
};

// 初始化
bool ModTsFile::threadInit(){
    return true;
};

// 清理
void ModTsFile::threadDeinit(){
    return;
}

// 线程工作单元
ModeTsFileStatus ModTsFile::threadRunning(){
    int loss_repeat_ans = 0;
    // 获取一个包，提前确认尚未到达尾部
    if(file_in_.eof()){
        Log::debug(__FILE__, __LINE__, "Success to reach the end of file");
        return ModeTsFileStatus::Stop;
    }
    file_in_.read((char*)buffer, TS_PACHET_LENGTH);

    int cur_position = 0;

    // 解析ts packet header
    TsPacketHeader* ts_packet_header = TsPacketHeader::parseTsPacketHeader(buffer, TS_PACHET_LENGTH);
    if(ts_packet_header == nullptr){
        Log::error(__FILE__, __LINE__, "Failed to parse the ts packet header");
        ModeTsFileStatus::Err;
    }
    cur_position += 4;

    // 查看是否存在自适应区
    // 如果还有adaption_field_control字段，注意：指定的是此段外的长度
    if(ts_packet_header->adaption_field_control == 0x2 || ts_packet_header->adaption_field_control == 0x3){
        cur_position += (unsigned)buffer[cur_position];
        cur_position ++;
    }
    // 没有adaption_filed_control
    else if(ts_packet_header->adaption_field_control == 0b01){
        cur_position += 0;
    }
    // 什么都没有，直接写入
    else{
        file_out_.write((const char*)buffer, TS_PACHET_LENGTH);
        return ModeTsFileStatus::Running;
    }


    // PAT 表
    if(ts_packet_header->pid == 0){
        // 如果是PSI表，而且payload_unit_start_indicator为1的情况下，会有一个字节的空填充
        if(ts_packet_header->payload_unit_start_indicator == 0b1){
            cur_position++;
        }
        TsPAT* new_pat = TsPAT::parsePAT(&buffer[cur_position], TS_PACHET_LENGTH - cur_position);
        if(new_pat != nullptr){
            Log::debug(__FILE__, __LINE__, "Success to read a pat");
            Log::debug(__FILE__, __LINE__, "this pat has %d programs", new_pat->program.size());

            if(pat_ != nullptr){
                delete pat_;
                pat_ = nullptr;
            }
            pat_ = new_pat;
            if(pat_->program.empty()){
                Log::debug(__FILE__, __LINE__, "pat has no program");
            }
            refreshPmtPidList();
        }
        else{
            delete new_pat;
            new_pat = nullptr;
            Log::error(__FILE__, __LINE__, "Failed to parse the pat table");
        }

        // 查看是否需要被删除
        loss_repeat_ans = shouldDeleteThisPack(Media::all, ts_packet_header->pid);
    }
    // 非PAT表
    else if(pat_ != nullptr){
        int pid = ts_packet_header->pid;
        // PMT表，更新PMT表
        if(isPmtPacket(pid)){
            // pmt也是PSI的一种，如果此处为start，也会填充一个空白字节
            if(ts_packet_header->payload_unit_start_indicator == 0b1){
                cur_position++;
            }
            TsPMT* cur_pmt = TsPMT::parsePMT(&buffer[cur_position], TS_PACHET_LENGTH - cur_position);
            if(cur_pmt != nullptr){
                // 查看是否存在，存在就更新，不存在就添加
                if(hasPmtNodeFromList(pid)){
                    // std::cout << "[DEBUG] has pmt for pid " << pid << ", do refresh" << std::endl;
                    refreshPmtNodeIntoList(pid, cur_pmt);
                }
                else{
                    pmt_pair_list_.push_back(std::make_pair(pid, cur_pmt));
                }
            }
            else{
                // 没能解析出来就不管了
            }
            // 查看是否需要删除
            // 但是不能直接删除，必须最后决定是否删除
            // 如果直接删除了，就无法拿到后续消息
            loss_repeat_ans = shouldDeleteThisPack(Media::all, pid);
            if(loss_repeat_ans < 0){
                Log::debug(__FILE__, __LINE__, "TEST: delete a pmt");
            }
        }
        // 当前是audio packet的情况，进行修改
        else if(isAudioPacket(pid)){
            program_pack_cnt++;

            // 查找pts是否成功，因为要返回绝对position，
            // 此处其实是从pes处开始查找
            std::pair<unsigned, unsigned> pts_pair =  findPtsfromPes(&buffer[cur_position], TS_PACHET_LENGTH - cur_position);
            // 对找到的进行修改
            
            int64_t final_pts_change = 0;
            if(pts_pair.first != 0){
                //std::cout << "Get pts time" << std::endl;
                int position = cur_position + pts_pair.first;
                int buffer_length = TS_PACHET_LENGTH - position;
                if(start_pts_ == 0){
                    start_pts_ = combinePts((char*)&buffer[position]);
                }
                cur_pts_ = combinePts((char*)&buffer[position]);
                cur_time_ = getTimeDiff(start_pts_, cur_pts_);
                Log::debug(__FILE__, __LINE__, "Cur Audio Time is %d", cur_time_);
                Log::debug(__FILE__, __LINE__, "Cur Audio Pts is %d", cur_pts_);

                if(end_time_ > 0 && cur_time_ > end_time_){
                    Log::debug(__FILE__, __LINE__, "Overtime: cur time is %d, cur pts is %d", cur_time_, cur_pts_);
                    return ModeTsFileStatus::Stop;
                }

                // 获取当前pts跳变情况
                int pts_change = getPtsChange(Media::audio, cur_pts_);
                final_pts_change += pts_change;

                // 如果不为0再进行修改
                if(final_pts_change != 0){
                    changePTS(&buffer[position], buffer_length, final_pts_change);
                    Log::debug(__FILE__, __LINE__, "Change pts at %d, add %d", cur_time_, final_pts_change);
                }

            }
            if(pts_pair.second != 0){
                int position = cur_position + pts_pair.second;
                int buffer_length = TS_PACHET_LENGTH - position;
                changePTS(&buffer[position], buffer_length, final_pts_change);
            }

            // 如果需要填充空包，就直接填充
            if(shouldFillWithNull(Media::audio)){
                writeNullPack((char*)buffer, TS_PACHET_LENGTH);
            }

            // 对时间进行更新后再决定对本包的处理
            loss_repeat_ans = shouldDeleteThisPack(Media::audio, pid);
        }
        // 当前是video packet的情况，进行修改
        else if(isVideoPacket(pid)){
            program_pack_cnt++;
            
            // 查找pts是否成功，因为要返回绝对position，因此
            std::pair<unsigned, unsigned> pts_pair = findPtsfromPes(&buffer[cur_position], TS_PACHET_LENGTH - cur_position);
            // 对找到的进行修改
            int64_t final_pts_change = 0;
            if(pts_pair.first != 0){
                //std::cout << "Get pts time" << std::endl;
                int position = cur_position + pts_pair.first;
                int buffer_length = TS_PACHET_LENGTH - position;
                if(start_pts_ == 0){
                    start_pts_ = combinePts((char*)&buffer[position]);
                }
                cur_pts_ = combinePts((char*)&buffer[position]);
                cur_time_ = getTimeDiff(start_pts_, cur_pts_);
                std::cout << "[DEBUG] Cur Video Time is " << cur_time_ << std::endl;
                std::cout << "[DEBUG] Cur Video Pts " << cur_pts_ << std::endl;
                if(end_time_ > 0 && cur_time_ > end_time_){
                    // 如果当前解析出来的时间已经超时了，就直接退出
                    return ModeTsFileStatus::Stop;
                }

                // 获取当前pts跳变情况
                int pts_change = getPtsChange(Media::video, cur_pts_);
                final_pts_change += pts_change;

                // 如果不为0再进行修改
                if(final_pts_change != 0){
                changePTS(&buffer[position], buffer_length, final_pts_change);
                std::cout << "[DEBUG] change pts at "<< cur_time_ << " add " << final_pts_change << std::endl;
                }
            }
            if(pts_pair.second != 0){
                int position = cur_position + pts_pair.second;
                int buffer_length = TS_PACHET_LENGTH - position;
                changePTS(&buffer[position], buffer_length, final_pts_change);
            }

            // 如果需要填充空包，就直接填充
            if(shouldFillWithNull(Media::video)){
                writeNullPack((char*)buffer, TS_PACHET_LENGTH);
            }

            // 对时间进行更新后再决定对本包的处理
            loss_repeat_ans = shouldDeleteThisPack(Media::video, pid);
        }
    }
    // 未解析出pat表，无法进行，直接跳过
    else{
        ;
    }



    // 如果需要重复
    if(loss_repeat_ans > 0){
        // std::cout << "[DEBUG] Success to repeate one pack at " << cur_time_ << std::endl;
        repeated_pack_cnt++;
        file_out_.write((const char*)buffer, TS_PACHET_LENGTH);
        file_out_.write((const char*)buffer, TS_PACHET_LENGTH);
    }
    // 如果需要删除，就跳不写入了
    else if(loss_repeat_ans < 0){
        lossed_pack_cnt++;
        std::cout << "[DEBUG] Success to loss one pack" << std::endl;
        return ModeTsFileStatus::Running;
    }
    // 如果没啥变化，就正常写入
    else{
        file_out_.write((const char*)buffer, TS_PACHET_LENGTH);
        return ModeTsFileStatus::Running;
    }
};

// 获得叠加的跳变
int64_t ModTsFile::getPtsChange(Media media_type,u_int64_t cur_pts){
    int64_t pts_change = 0;
    for(auto iter = pts_pattern_list_.begin(); iter != pts_pattern_list_.end(); iter++){
        FuncPattern* pattern = *iter;
        // 超时的pts pattern删除
        if(cur_time_ > pattern->end_sec){
            std::cout << "At sec" << cur_time_ <<  ", pattern overtime, so do delete" << std::endl;
            delete pattern;
            pattern = nullptr;
            pts_pattern_list_.erase(iter);
            iter--;
        }
        // 不符合条件的跳过
        else if(pattern->media != Media::all && pattern->media != media_type){
            continue;
        }
        // 符合条件的操作
        else if(cur_time_ >= pattern->start_sec && cur_time_ <= pattern->end_sec){
            int64_t base_pts = pattern->pts_base; 
            PtsFunc func = pattern->pts_func;
            int i = 0;
            switch (func)
            {
            case PtsFunc::add:
                pts_change += base_pts;
                break;
            case PtsFunc::minus:
                pts_change -= base_pts;
                break;
            case PtsFunc::sin:
                pts_change += sin(cur_time_) * base_pts;
                break;
            case PtsFunc::pulse:
                i = (sin(cur_time_) >= 0) ? 1 : -1;
                pts_change += i * base_pts;
                break;
            case PtsFunc::mult:
                // 这里混用 int 和 uint 其实有一些风险，但是pts应该不会超过这个值
                pts_change += (pattern->pts_base * static_cast<int64_t>(cur_pts)) - static_cast<int64_t>(cur_pts);
                std::cout << "[DEBUG] get a mult func at " << cur_time_ << ", pts change is " << pts_change <<  "and cur pts is " << cur_pts << std::endl;
                break;
            default:
                std::cout << "[DEBUG] No pattern compare" << std::endl;
                break;
            }
            // std::cout << "[DEBUG] After change, now pts_change is " << pts_change << std::endl;
        }
    }
    return pts_change;
}


// 修改PTS，增加pts_change值
bool ModTsFile::changePTS(u_char* buffer, int buffer_length, int64_t pts_change){
    if(buffer == nullptr){
        return false;
    }

    //将pts中的pts生成int_64_t
    int64_t pts_num = combinePts((char*)buffer);
    // 修改pts
    pts_num += pts_change;
    // 重新写回 payload�?
    rewritePts((char*)buffer, pts_num);
    return true;
}

// 修改DTS，增加pts_change值
bool ModTsFile::changeDTS(u_char* buffer, int buffer_length, int64_t dts_change){
    if(buffer == nullptr){
        return false;
    }

    //将pts中的pts生成int_64_t
    int64_t pts_num = combinePts((char*)buffer);
    // 修改pts
    pts_num += dts_change;
    // 重新写回 payload�?
    rewritePts((char*)buffer, pts_num);
    return true;
}

// 覆写pts
void ModTsFile::rewritePts(char* payload, u_int64_t pts){
    // 清空buffer
    for(int i = 0; i < 5; i++)
        payload[i] = 0;
    // 传入对应的内�?
    payload[0] = (char)((pts >> 29) & 0x0E);
    payload[1] = (char)((pts >> 22) & 0xFF);
    payload[2] = (char)((pts >> 14) & 0xFE);
    payload[3] = (char)((pts >> 7) & 0xFF);
    payload[4] = (char)((pts << 1) & 0xFE);

    // 添加标记�?
    payload[0] |= 0b00110001;
    payload[2] |= 0b00000001;
    payload[4] |= 0b00000001;
    // 返回
    return ;
}

// 覆写dts
void ModTsFile::rewriteDts(char* payload, u_int64_t dts){
    // 清空payload
    for(int i = 0; i < 5; i++)
        payload[i] = 0;
    // 传入对应的内�?
    payload[0] = (char)((dts >> 29) & 0x0E);
    payload[1] = (char)((dts >> 22) & 0xFF);
    payload[2] = (char)((dts >> 14) & 0xFE);
    payload[3] = (char)((dts >> 7) & 0xFF);
    payload[4] = (char)((dts << 1) & 0xFE);

    // 添加标记�?
    payload[0] |= 0b00010001;
    payload[2] |= 0b00000001;
    payload[4] |= 0b00000001;
    // 返回
    return ;
}

// 查看是否成功
bool ModTsFile::Success(){
    return be_success_;
}

std::pair<unsigned, unsigned> ModTsFile::findPtsfromPes(u_char* buffer, int buffer_length){
    if(buffer == nullptr){
        return std::make_pair(0,0);
    }
    int cur_position = 0;
    // 解析pes头部，一共5字节
    PesPacketHeader pes_header;
    memcpy(&pes_header, buffer, 5);

    // 当前包没有pes�?
    if(pes_header.start_code_prefix[0] != 0x00 ||
    pes_header.start_code_prefix[1] != 0x00 ||
    pes_header.start_code_prefix[2] != 0x01){
        return std::make_pair(0,0); 
    }
    // 这几类stream_id不解析pts
    else if(pes_header.stream_id[0] == STREAM_ID_PROGRAM_STREAM_MAP ||
            pes_header.stream_id[0] == STREAM_ID_PADDING_STREAM || 
            pes_header.stream_id[0] == STREAM_ID_PRIVATE_STREAM_2 || 
            pes_header.stream_id[0] == STREAM_ID_ECM_STREAM ||
            pes_header.stream_id[0] == STREAM_ID_EMM_STREAM ||
            pes_header.stream_id[0] == STREAM_ID_PROGRAM_STREAM_DIRECTORY ||
            pes_header.stream_id[0] == STREAM_ID_DSMCC_STREAM || 
            pes_header.stream_id[0] == STREAM_ID_H222_E_STREAM
    ){
        static int err_pes_type_cnt = 0;
        std::cout << "[DEBUG] Err Pes type: " << err_pes_type_cnt++ << std::endl;
        return std::make_pair(0,0); 
    }
    else{
        cur_position += 6;
    }
    
    // 查看是否有pes包的adaption headers
    OptionalPesHeader* optional_pes_header = OptionalPesHeader::parseOptionalPesHeader(&buffer[cur_position], buffer_length - cur_position);
    if(optional_pes_header == nullptr){
        return std::make_pair(0,0);
    }


    // 如果开头不�?2，就不是pes adaptation header
    if(optional_pes_header->marker_bits != 2){
        return std::make_pair(0,0);
    }
    // 虽然有pes adaptation header，但是pts_dts指示表示并不存在pts_dts
    else if(optional_pes_header->pts_dts_indicator == 0b00){
        return std::make_pair(0,0);
    }

    // 有optional header与pts/dts，先移动位置
    cur_position += 3;
    // dts_pts同时存在
    static int err_pts_cnt = 0;
    if(optional_pes_header->pts_dts_indicator == 0b11){
        u_char temp_buffer = buffer[cur_position];
        if(verifyHighFourBits(temp_buffer, (const u_char)0b11))
            return std::make_pair(cur_position, cur_position+5);
        else{
            std::cout << "[DEBUG] Errrrrrr pts dts! " << err_pts_cnt++ << std::endl;
            return std::make_pair(0, 0);
        }
            
    }
    // 只有dts存在
    else if(optional_pes_header->pts_dts_indicator == 0b10){
        u_char temp_buffer = buffer[cur_position];
        if(verifyHighFourBits(temp_buffer, (const u_char)0b10))
            return std::make_pair(cur_position, cur_position+5);
        else{
            std::cout << "[DEBUG] Errrrrrr just dts! " << err_pts_cnt++ << std::endl;
            return std::make_pair(0, 0);
        }

            

        return std::make_pair(cur_position, 0);
    }
    else
        return std::make_pair(0,0);
}

// 从字节中获取pts的数�?/其实pts和dts储存数值的位置都是一样的
u_int64_t ModTsFile::combinePts(char buffer[5]) {
    u_int64_t pts_num = 0;
    pts_num |= ((u_int64_t)buffer[0] & 0b00001110) << 29;
    pts_num |= ((u_int64_t)buffer[1] & 0b11111111) << 22;
    pts_num |= ((u_int64_t)buffer[2] & 0b11111110) << 14;
    pts_num |= ((u_int64_t)buffer[3] & 0b11111111) << 7;
    pts_num |= ((u_int64_t)buffer[4] & 0b11111110) >> 1;
    return pts_num;
}


// 去除后缀
std::string ModTsFile::removeSuffix(const std::string& str, const std::string& suffix)
{
    if (str.length() >= suffix.length() && str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0) {
        return str.substr(0, str.length() - suffix.length());
    }
    return str;
}

// 更换输出文件�?
bool ModTsFile::changeOutoutFile(std::ofstream* file_out, std::string& file_name, int slice_count){
    file_out_.close();
    std::string new_file_name = removeSuffix(file_name, ".ts") + "_" + std::to_string(slice_count) + ".ts";
    file_out_.open(new_file_name.c_str(), std::ios_base::out);
    if(!file_out_.is_open()){
        std::cerr << "Failed to open the file "  << new_file_name << std::endl;
        return false;
    }
    return true;
}

// 根据pts差，计算时间差（s）
double ModTsFile::getTimeDiff(u_int64_t start_pts_, u_int64_t end_pts){
    int64_t time_diff = 0;
    if(end_pts > start_pts_){
        time_diff = end_pts - start_pts_;
        return (double)time_diff / 90000;
    }
    else{
        time_diff = start_pts_ - end_pts;
        return -((double)time_diff / 90000);
    }
}

// 依照PAT对pmt list 进行一轮整理，删除pat中不存在的pmt
void ModTsFile::refreshPmtPidList(){
    // 获取pat中的program的PMT pid列表pat_pmt_pid_list
    std::vector<int> pat_pmt_pid_list;
    for(auto iter : pat_->program){
        int i = iter.network_program_map_PID;
        pat_pmt_pid_list.push_back(i);
    }

    // 删除现在不存在的pmt pid
    for(auto iter = pmt_pair_list_.begin(); iter != pmt_pair_list_.end(); iter++){
        int cur_pid = (*iter).first;
        // pat的pid列表中没找到的PMT pid，需要删除
        if(std::find(pat_pmt_pid_list.begin(), pat_pmt_pid_list.end(), cur_pid) == pat_pmt_pid_list.end()){
            TsPMT* temp_pmt = (*iter).second;
            delete temp_pmt;
            temp_pmt = nullptr;
            pmt_pair_list_.erase(iter);
            iter--;
        }
    }
}

// 查找pid是否为pmt
bool ModTsFile::isPmtPacket(const int pid){
    if(pat_ != nullptr){
        for(auto iter : pat_->program){
            if(pid == iter.network_program_map_PID){
                return true;
            }
        }
    }
    return false;
}

// 查找pid是否为视频
bool ModTsFile::isAudioPacket(const int pid){
    if(pat_ == nullptr){
        return false;
    }
    else if(isPmtPacket(pid)){
        return false;
    }
    // 查找当前解析出的所有pmt列表

    bool findPid = false;
    uint8_t type = 0;

    for(auto pmt_pair : pmt_pair_list_){
        auto pmt = pmt_pair.second;
        if(pmt == nullptr){
            ;
        }
        else{
            // 遍历所有的program
            for(auto program : pmt->PMT_Stream){
                if(pid == program.elementary_PID){
                    findPid = true;
                    type = program.stream_type;
                }
            }
        }
    }
    // 如果找到pid了，储存了对应的streamtype
    if(findPid){
        return isAudioStream(type);
    }
    else{
        return false;
    }
}

// 查找pid是否为音频
bool ModTsFile::isVideoPacket(const int pid){
    if(pat_ == nullptr){
        return false;
    }
    else if(isPmtPacket(pid)){
        return false;
    }
    // 查找当前解析出的所有pmt列表

    bool findPid = false;
    uint8_t type = 0;

    for(auto pmt_pair : pmt_pair_list_){
        auto pmt = pmt_pair.second;
        if(pmt == nullptr){
            ;
        }
        else{
            // 遍历所有的program
            for(auto program : pmt->PMT_Stream){
                if(pid == program.elementary_PID){
                    findPid = true;
                    type = program.stream_type;
                }
            }
        }
    }
    // 如果找到pid了，储存了对应的streamtype
    if(findPid){
        return isVideoStream(type);
    }
    else{
        return false;
    }
}

// 从当前已经解析的列表中查找pid是否存在
bool ModTsFile::hasPmtNodeFromList(const int pid){
    for(auto pmt_pair : pmt_pair_list_){
        if(pid == pmt_pair.first){
            return true;
        }
    }
    return false;
}

// 更新pmt Node的信息
bool ModTsFile::refreshPmtNodeIntoList(const int pid, TsPMT* pmt){
    for(auto iter = pmt_pair_list_.begin(); iter != pmt_pair_list_.end(); iter++){
        auto pmt_pair = *iter;
        if(pid == pmt_pair.first){
            pmt_pair_list_.erase(iter);
            pmt_pair_list_.push_back(std::make_pair(pid, pmt));
            return true;
        }
    }
    return false;
}

// 判断当前类型是否为视频
bool ModTsFile::isVideoStream(uint8_t streamType) {
    switch(streamType) {
        case STREAMTYPE_11172_VIDEO:
        case STREAMTYPE_13818_VIDEO:
        case STREAMTYPE_H264_VIDEO:
        case STREAMTYPE_AVS_VIDEO:
            return true;
        default:
            return false;
    }
}

// 判断当前类型是否为音频
bool ModTsFile::isAudioStream(uint8_t streamType) {
    switch(streamType) {
        case STREAMTYPE_11172_AUDIO:
        case STREAMTYPE_13818_AUDIO:
        case STREAMTYPE_AC3_AUDIO:
        case STREAMTYPE_AAC_AUDIO:
        case STREAMTYPE_MPEG4_AUDIO:
            return true;
        default:
            return false;
    }
}

// 返回-1，代表删除，返回0，代表不变，返回1，代表重复
int ModTsFile::shouldDeleteThisPack(Media media_type, int pid){
    int ans = 0;
    
    // 空的时候返回0
    if(loss_repeat_list_.empty()){
        return 0;
    }

    // 循环次数
    for(auto iter = loss_repeat_list_.begin(); iter != loss_repeat_list_.end() ; iter++){
        FuncPattern* pattern = *iter;
        if(cur_time_ >  pattern->end_sec){
            std::cout << "At sec" << cur_time_ <<  ", pattern overtime, so do delete" << std::endl;
            delete pattern;
            pattern = nullptr;
            loss_repeat_list_.erase(iter);
            iter--;
        }
        // 不符合条件的跳过
        else if(pattern->media != Media::all && pattern->media != media_type){
            continue;
        }
        // 符合条件的执行操作
        else if(cur_time_ >= pattern->start_sec && cur_time_ <=  pattern->end_sec){
            // random delete 
            if(pattern->pts_func == PtsFunc::loss){
                std::cout << "[DEBUG] get a loss func" << std::endl;
                int rate = static_cast<int>(pattern->pts_base) % 100;
                int rand = std::rand() % 100;
                if(rand <= rate || pattern->pts_base >= 100){
                    std::cout << "[DEBUG] delete a packet at pid" << pid << std::endl;
                    ans = -1;
                }
                else{
                    ans = 0;
                }
            }
            else if(pattern->pts_func == PtsFunc::repeate){
                std::cout << "[DEBUG] get a repeate func" << std::endl;
                int rate = static_cast<int>(pattern->pts_base) % 100;
                int rand = std::rand() % 100;
                if(rand <= rate || pattern->pts_base >= 100){
                    std::cout << "[DEBUG] repeate a packet at pid" << pid << std::endl;
                    ans = 1;
                }
                else{
                    ans = 0;
                }
            }
            else if(pattern->pts_func == PtsFunc::pat_delete){
                std::cout << "[DEBUG] get a pat_delete func" << std::endl;
                if(pid == 0){
                    std::cout << "[DEBUG] delete a PAT at pid" << pid << std::endl;
                    ans = -1;
                }
                else{
                    ans = 0;
                }
            }
            else if(pattern->pts_func == PtsFunc::pmt_delete){
                std::cout << "[DEBUG] get a pat_delete func" << std::endl;
                if(isPmtPacket(pid)){
                    std::cout << "[DEBUG] delete a PMT at pid" << pid << std::endl;
                    ans = -1;
                }
                else{
                    ans = 0;
                }
            }
            else{
                std::cout << "get a err func" << std::endl;
            }
        }
    }
    
    return ans;
}

// 查看是否需要填充空包
bool ModTsFile::shouldFillWithNull(Media media_type){
    if(ts_err_list_.empty()){
        return false;
    }
    // 循环次数
    for(auto iter = ts_err_list_.begin(); iter != ts_err_list_.end() ; iter++){
        FuncPattern* pattern = *iter;
        // 删除超时部分
        if(cur_time_ >  pattern->end_sec){
            std::cout << "At sec" << cur_time_ <<  ", pattern overtime, so do delete" << std::endl;
            delete pattern;
            pattern = nullptr;
            ts_err_list_.erase(iter);
            iter--;
        }
        // 不符合条件的跳过
        else if(pattern->media != Media::all && pattern->media != media_type){
            continue;
        }
        // 符合条件的执行操作
        else if(cur_time_ >= pattern->start_sec && cur_time_ <=  pattern->end_sec){
            // 返回可以填充空包的结果
            if(pattern->pts_func == PtsFunc::null){
                return true;
            }
        }
    }
    return false;
}

// 从头开始写空包
void ModTsFile::writeNullPack(char* payload, int buffer_length){
    static long long null_pack_cnt = 0;
    std::cout << "[DEBUG] Write " << null_pack_cnt++ << " nullpack" << std::endl;
    if(payload == nullptr || buffer_length < 188){
        return;
    }
    // 解析头部，重新写入头部
    TsPacketHeader header;
    header.parseTsPacketHeader((u_char*)payload, buffer_length);
    header.sync_byte = 0x47;
    header.transport_error_indicator = 0;
    header.payload_unit_start_indicator = 0;
    header.transport_priority = 0;
    header.transport_scrambling_control = 0;
    header.adaption_field_control = 0x01;
    TsPacketHeader::writeTsPacketHeaderToChar(&header, payload);

    // 填充剩余的空包
    memset((void*)(payload + 4), 0, buffer_length - 4);
}

/*------------------------------- Debugging Func -------------------------------*/
// debugging用，输出byte
void printBinaryDigits(unsigned char byte) {
    for (int i = 7; i >= 0; --i) {
        int bit = (byte >> i) & 1;
        std::cout << bit;
    }
}
