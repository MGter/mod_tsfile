#include "mod_ts_file.h"
#include <iostream>
#include "common/Log.h"

/*------------------------------- Base Struct -------------------------------*/
TsPAT* TsPAT::parsePAT(u_char* buffer, int buffer_length) {
    if(buffer == nullptr || buffer_length < 8){
        return nullptr;
    }

    TsPAT* packet = new TsPAT();

    packet->table_id                    = buffer[0];
    packet->section_syntax_indicator    = buffer[1] >> 7;
    packet->zero                        = (buffer[1] >> 6) & 0x1;

    // PAT表的table_id必须为0，section_syntax_indicator必须为1，zero必须为0
    if(packet->table_id != 0 || packet->section_syntax_indicator != 1 || packet->zero != 0){
        delete packet;
        return nullptr;
    }

    packet->reserved_1                  = (buffer[1] >> 4) & 0x3;
    packet->section_length              = ((buffer[1] & 0x0F) << 8) | buffer[2];

    // 缓冲区长度检查：section_length + 3（table_id等3字节）
    int total_len = 3 + packet->section_length;
    if(buffer_length < total_len || packet->section_length < 9){
        delete packet;
        return nullptr;
    }

    packet->transport_stream_id         = (buffer[3] << 8) | buffer[4];
    packet->reserved_2                  = buffer[5] >> 6;
    packet->version_number              = (buffer[5] >> 1) & 0x1F;
    packet->current_next_indicator      = buffer[5] & 0x01;
    packet->section_number              = buffer[6];
    packet->last_section_number         = buffer[7];

    // CRC在section末尾4字节
    packet->CRC_32 = (buffer[total_len - 4] & 0xFF) << 24 |
                     (buffer[total_len - 3] & 0xFF) << 16 |
                     (buffer[total_len - 2] & 0xFF) << 8 |
                     (buffer[total_len - 1] & 0xFF);

    // program info从buffer[8]开始，每个program占4字节
    // section_length - 9（除去头8字节和CRC4字节前1字节）= program信息长度
    // 实际：section_length - 4(CRC) - 5(transport_stream_id等) = program长度
    int program_loop_len = packet->section_length - 9;  // 4 + 5 = 9

    for (int n = 0; n + 4 <= program_loop_len; n += 4) {
        // 防止越界访问
        if(8 + n + 3 >= buffer_length){
            break;
        }

        unsigned program_num = (buffer[8 + n] << 8) | buffer[8 + n + 1];

        if (program_num == 0x00) {
            // Network Information Table
            packet->reserved_3 = buffer[8 + n + 2] >> 5;
            packet->network_PID = ((buffer[8 + n + 2] & 0x1F) << 8) | buffer[8 + n + 3];
        } else {
            Program PAT_program;
            PAT_program.program_number = program_num;
            PAT_program.reserved = buffer[8 + n + 2] >> 5;
            PAT_program.network_program_map_PID = ((buffer[8 + n + 2] & 0x1F) << 8) | buffer[8 + n + 3];
            packet->program.push_back(PAT_program);
        }
    }
    return packet;
}

TsPMT* TsPMT::parsePMT(u_char* buffer, int buffer_length)
{
    if(buffer == nullptr || buffer_length < 12){
        return nullptr;
    }

    TsPMT* packet = new TsPMT;

    packet->table_id = buffer[0];
    // PMT的table_id必须为0x02
    if(packet->table_id != 0x02){
        delete packet;
        return nullptr;
    }

    packet->section_syntax_indicator = buffer[1] >> 7;
    packet->zero = (buffer[1] >> 6) & 0x01;
    packet->reserved_1 = (buffer[1] >> 4) & 0x03;
    packet->section_length = ((buffer[1] & 0x0F) << 8) | buffer[2];

    // 缓冲区长度检查
    int total_len = packet->section_length + 3;
    if(buffer_length < total_len || packet->section_length < 9){
        delete packet;
        return nullptr;
    }

    packet->program_number = (buffer[3] << 8) | buffer[4];
    packet->reserved_2 = buffer[5] >> 6;
    packet->version_number = (buffer[5] >> 1) & 0x1F;
    packet->current_next_indicator = buffer[5] & 0x01;
    packet->section_number = buffer[6];
    packet->last_section_number = buffer[7];
    packet->reserved_3 = buffer[8] >> 5;
    packet->PCR_PID = ((buffer[8] & 0x1F) << 8) | buffer[9];

    packet->reserved_4 = buffer[10] >> 4;
    packet->program_info_length = ((buffer[10] & 0x0F) << 8) | buffer[11];

    // 检查program_info_length是否会导致越界
    if(12 + packet->program_info_length > buffer_length){
        delete packet;
        return nullptr;
    }

    // CRC在section末尾
    packet->CRC_32 = (buffer[total_len - 4] << 24) |
                     (buffer[total_len - 3] << 16) |
                     (buffer[total_len - 2] << 8) |
                     buffer[total_len - 1];

    int pos = 12;

    // 跳过program info descriptor
    if (packet->program_info_length != 0){
        pos += packet->program_info_length;
    }

    // 解析stream info，CRC占4字节在末尾
    int stream_info_end = total_len - 4;

    while (pos + 5 <= stream_info_end && pos + 5 <= buffer_length)
    {
        TsPmtStream pmt_stream;
        pmt_stream.stream_type = buffer[pos];
        packet->reserved_5 = buffer[pos + 1] >> 5;
        pmt_stream.elementary_PID = ((buffer[pos + 1] & 0x1F) << 8) | buffer[pos + 2];
        packet->reserved_6 = buffer[pos + 3] >> 4;
        pmt_stream.ES_info_length = ((buffer[pos + 3] & 0x0F) << 8) | buffer[pos + 4];

        // 检查ES_info_length是否会导致越界
        if(pos + 5 + pmt_stream.ES_info_length > buffer_length){
            break;
        }

        pmt_stream.descriptor = 0x00;
        if (pmt_stream.ES_info_length != 0 && pos + 5 <= buffer_length)
        {
            pmt_stream.descriptor = buffer[pos + 5];
            for (int i = 2; i <= pmt_stream.ES_info_length && pos + 4 + i < buffer_length; i++)
            {
                pmt_stream.descriptor = (pmt_stream.descriptor << 8) | buffer[pos + 4 + i];
            }
        }

        pos += 5 + pmt_stream.ES_info_length;
        packet->PMT_Stream.push_back(pmt_stream);
    }

    return packet;
}

TsPacketHeader* TsPacketHeader::parseTsPacketHeader(u_char* buffer, int buffer_length) {
    if(buffer == nullptr || buffer_length < 4){
        Log::error(__FILE__, __LINE__, "Null buffer or Invalid buffer length");
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
        Log::error(__FILE__, __LINE__, "Null buffer or Optional Pes header out of buffer length");
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
    statu_ = ModTsFileStatus::Stop;

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
    Log::info(__FILE__, __LINE__, "Success to start a thread");
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

// 清理list的逻辑，指针已经分散到各个列表中，只需清理各列表即可
bool ModTsFile::clearParamList(){
    // 注意：指针已在 arrangeParamList() 中转移到各列表
    // 不要重复 delete task_param_->func_pattern 的元素

    if(cut_param_ != nullptr){
        delete cut_param_;
        cut_param_ = nullptr;
    }

    for(auto param : pts_pattern_list_){
        if(param != nullptr){
            delete param;
        }
    }
    pts_pattern_list_.clear();

    for(auto param : loss_repeat_list_){
        if(param != nullptr){
            delete param;
        }
    }
    loss_repeat_list_.clear();

    for(auto param : ts_err_list_){
        if(param != nullptr){
            delete param;
        }
    }
    ts_err_list_.clear();

    // 清空 task_param 的指针列表（不 delete，因为指针已转移）
    if(task_param_ != nullptr){
        task_param_->func_pattern.clear();
    }

    return true;
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
    statu_ = ModTsFileStatus::Running;
    while(statu_ != ModTsFileStatus::Stop){
        std::lock_guard<std::mutex> locker(thread_mut);
        switch (statu_)
        {
        case ModTsFileStatus::Running:
            statu_ = threadRunning();
            break;
        case ModTsFileStatus::Err:
            statu_ = ModTsFileStatus::Stop;
            break;
        default:
            statu_ = ModTsFileStatus::Stop;
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
    Log::info(__FILE__, __LINE__, "get ts cnt: %d", get_packet_cnt);
    Log::info(__FILE__, __LINE__, "write ts cnt: %d", write_packet_cnt);

    Log::info(__FILE__, __LINE__, "pat packet cnt: %d", pat_packet_cnt);
    Log::info(__FILE__, __LINE__, "pmt packet cnt: %d", pmt_packet_cnt);
    Log::info(__FILE__, __LINE__, "audio packet cnt: %d", audio_pack_cnt);
    Log::info(__FILE__, __LINE__, "video packet cnt: %d", video_pack_cnt);
    
    Log::info(__FILE__, __LINE__, "unknown packet cnt: %d", unknown_packet_cnt);

    return;
}

// 线程工作单元
ModTsFileStatus ModTsFile::threadRunning(){
    int loss_repeat_ans = 0;        // 判断是否增删文件
    int cur_position = 0;           // 当前包处理位置

    // 获取文件
    if(file_in_.eof()){
        Log::info(__FILE__, __LINE__, "Success to reach the end of file, get %d ts packet", get_packet_cnt);
        return ModTsFileStatus::Stop;
    }
    file_in_.read((char*)buffer, TS_PACKET_LENGTH);
    get_packet_cnt++;   // 计算当前读入的ts_packet数量

    // 检查读取是否成功
    if(file_in_.gcount() != TS_PACKET_LENGTH){
        Log::info(__FILE__, __LINE__, "Incomplete packet read, reached end of file");
        return ModTsFileStatus::Stop;
    }

    // 解析ts头（使用栈上对象避免内存泄漏）
    TsPacketHeader ts_header;
    u_char* buf_ptr = buffer;
    ts_header.sync_byte = buf_ptr[0];
    ts_header.transport_error_indicator = (buf_ptr[1] >> 7) & 0x01;
    ts_header.payload_unit_start_indicator = (buf_ptr[1] >> 6) & 0x01;
    ts_header.transport_priority = (buf_ptr[1] >> 5) & 0x01;
    ts_header.pid = ((buf_ptr[1] & 0x1F) << 8) | buf_ptr[2];
    ts_header.transport_scrambling_control = (buf_ptr[3] >> 6) & 0x03;
    ts_header.adaption_field_control = (buf_ptr[3] >> 4) & 0x03;
    ts_header.continuity_counter = buf_ptr[3] & 0x0F;

    // 验证同步字节（必须是0x47）
    if(ts_header.sync_byte != 0x47){
        Log::error(__FILE__, __LINE__, "Invalid sync byte: 0x%02X, expected 0x47", ts_header.sync_byte);
        unknown_packet_cnt++;
        file_out_.write((const char*)buffer, TS_PACKET_LENGTH);
        write_packet_cnt++;
        return ModTsFileStatus::Running;  // 继续处理，可能是损坏的包
    }

    // 检查传输错误指示器
    if(ts_header.transport_error_indicator == 1){
        Log::debug(__FILE__, __LINE__, "Transport error indicator set, packet may be corrupted");
    }

    TsPacketHeader* ts_packet_header = &ts_header;
    cur_position += 4;

    // 跳过自适应区（需要检查cur_position是否越界）
    if(ts_packet_header->adaption_field_control == 0x2 || ts_packet_header->adaption_field_control == 0x3){
        int adaptation_field_length = (int)buffer[cur_position];
        // 自适应区长度不应该超过包剩余空间
        if(cur_position + 1 + adaptation_field_length > TS_PACKET_LENGTH){
            Log::error(__FILE__, __LINE__, "Invalid adaptation field length: %d", adaptation_field_length);
            unknown_packet_cnt++;
            file_out_.write((const char*)buffer, TS_PACKET_LENGTH);
            write_packet_cnt++;
            return ModTsFileStatus::Running;
        }
        cur_position += adaptation_field_length;
        cur_position ++;
    }
    // 有负载
    else if(ts_packet_header->adaption_field_control == 0b01){
        cur_position += 0;
    }
    // 无负载（adaptation_field_control == 0x00）
    else{
        unknown_packet_cnt++;
        file_out_.write((const char*)buffer, TS_PACKET_LENGTH);
        write_packet_cnt++;
        return ModTsFileStatus::Running;
    }

    // 检查负载位置是否有效
    if(cur_position >= TS_PACKET_LENGTH){
        Log::error(__FILE__, __LINE__, "Payload start position out of bounds: %d", cur_position);
        unknown_packet_cnt++;
        file_out_.write((const char*)buffer, TS_PACKET_LENGTH);
        write_packet_cnt++;
        return ModTsFileStatus::Running;
    }

    // PAT 表
    if(ts_packet_header->pid == 0){
        pat_packet_cnt++;
        // 如果是PSI表，而且payload_unit_start_indicator为1的情况下，会有一个字节的空填充
        if(ts_packet_header->payload_unit_start_indicator == 0b1){
            cur_position++;
        }
        TsPAT* new_pat = TsPAT::parsePAT(&buffer[cur_position], TS_PACKET_LENGTH - cur_position);
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

        // 确认是否需要被删除
        loss_repeat_ans = shouldDeleteThisPack(Media::all, ts_packet_header->pid);
    }
    // 非PAT表
    else if(pat_ != nullptr){
        int pid = ts_packet_header->pid;
        // PMT表，更新PMT表
        if(isPmtPacket(pid)){
            pmt_packet_cnt++;
            // pmt也是PSI的一种，如果此处为start，也会填充一个空白字节
            if(ts_packet_header->payload_unit_start_indicator == 0b1){
                cur_position++;
            }
            TsPMT* cur_pmt = TsPMT::parsePMT(&buffer[cur_position], TS_PACKET_LENGTH - cur_position);
            if(cur_pmt != nullptr){
                if(hasPmtNodeFromList(pid)){
                    refreshPmtNodeIntoList(pid, cur_pmt);
                }
                else{
                    pmt_pair_list_.push_back(std::make_pair(pid, cur_pmt));
                }
            }
            else{
                // 没能解析出来就不管了
            }

            // 查看是否删除此ts包
            loss_repeat_ans = shouldDeleteThisPack(Media::all, pid);
        }
        // audio包
        else if(isAudioPacket(pid)){
            audio_pack_cnt++;

            // 获取pts时间
            std::pair<unsigned, unsigned> pts_pair = std::pair<unsigned, unsigned>(0,0);
            if(ts_packet_header->payload_unit_start_indicator == 0b1){
                Log::info(__FILE__, __LINE__, "Gat a audio pes load header!");
                pts_pair = findPtsfromPes(&buffer[cur_position], TS_PACKET_LENGTH - cur_position);
            }
            
            int64_t final_pts_change = 0;
            if(pts_pair.first != 0){
                Log::debug(__FILE__, __LINE__, "Gat a audio pts!");
                int position = cur_position + pts_pair.first;
                int buffer_length = TS_PACKET_LENGTH - position;
                if(start_pts_ == 0){
                    start_pts_ = combinePts((char*)&buffer[position]);
                }
                cur_pts_ = combinePts((char*)&buffer[position]);
                cur_time_ = getTimeDiff(start_pts_, cur_pts_);
                Log::debug(__FILE__, __LINE__, "Cur audio time: %f, pts: ", cur_time_, cur_pts_);
                if(end_time_ > 0 && cur_time_ > end_time_){
                    Log::info(__FILE__, __LINE__, "Over time at %f, pts is %d end time is %d so stop", cur_time_, cur_pts_, end_time_);
                    return ModTsFileStatus::Stop;
                }

                // 获取当前pts跳变情况
                int64_t pts_change = getPtsChange(Media::audio, cur_pts_);
                final_pts_change += pts_change;

                // 对需要进行修改pts的部分进行修改
                if(final_pts_change != 0){
                    changePTS(&buffer[position], buffer_length, final_pts_change);
                    Log::info(__FILE__, __LINE__, "Change pts at %f, add %lld", cur_time_, final_pts_change);
                }
            }
            if(pts_pair.second != 0){
                int position = cur_position + pts_pair.second;
                int buffer_length = TS_PACKET_LENGTH - position;
                changePTS(&buffer[position], buffer_length, final_pts_change);
            }

            // 如果需要填充空包，就直接填充
            if(shouldFillWithNull(Media::audio)){
                Log::info(__FILE__, __LINE__, "Fill with a null pack to a audio pack");
                writeNullPack((char*)buffer, TS_PACKET_LENGTH);
            }

            // 对时间进行更新后再决定对本包的处理
            loss_repeat_ans = shouldDeleteThisPack(Media::audio, pid);
        }
        // 当前是video packet的情况，进行修改
        else if(isVideoPacket(pid)){
            video_pack_cnt++;

            std::pair<unsigned, unsigned> pts_pair = std::pair<unsigned, unsigned>(0,0);
            if(ts_packet_header->payload_unit_start_indicator == 0b1){
                Log::info(__FILE__, __LINE__, "Gat a video pes load header!");
                pts_pair = findPtsfromPes(&buffer[cur_position], TS_PACKET_LENGTH - cur_position);
            }

            int64_t final_pts_change = 0;
            if(pts_pair.first != 0){
                Log::debug(__FILE__, __LINE__, "Gat a video pts!");
                int position = cur_position + pts_pair.first;
                int buffer_length = TS_PACKET_LENGTH - position;
                if(start_pts_ == 0){
                    start_pts_ = combinePts((char*)&buffer[position]);
                }
                cur_pts_ = combinePts((char*)&buffer[position]);
                cur_time_ = getTimeDiff(start_pts_, cur_pts_);

                Log::debug(__FILE__, __LINE__, "Cur video time: %f, pts: ", cur_time_, cur_pts_);
                if(end_time_ > 0 && cur_time_ > end_time_){
                    Log::info(__FILE__, __LINE__, "Over time at %f, pts is %d end time is %d so stop", cur_time_, cur_pts_, end_time_);
                    return ModTsFileStatus::Stop;
                }

                // 获取当前pts跳变情况
                int64_t pts_change = getPtsChange(Media::video, cur_pts_);
                if(pts_change != 0){
                    final_pts_change += pts_change;
                }
                

                // 如果不为0再进行修改
                if(final_pts_change != 0){
                    changePTS(&buffer[position], buffer_length, final_pts_change);
                    Log::info(__FILE__, __LINE__, "Change pts at %f, add %lld", cur_time_, final_pts_change);
                }
            }
            if(pts_pair.second != 0){
                Log::debug(__FILE__, __LINE__, "Gat a video dts!");
                int position = cur_position + pts_pair.second;
                int buffer_length = TS_PACKET_LENGTH - position;
                changePTS(&buffer[position], buffer_length, final_pts_change);
            }

            // 如果需要填充空包，就直接填充
            if(shouldFillWithNull(Media::video)){
                Log::info(__FILE__, __LINE__, "Fill with a null pack to a video pack");
                writeNullPack((char*)buffer, TS_PACKET_LENGTH);
            }

            // 对时间进行更新后再决定对本包的处理
            loss_repeat_ans = shouldDeleteThisPack(Media::video, pid);
        }
        else{
            unknown_packet_cnt++;
        }
    }
    // 未解析出pat表，无法进行，直接跳过
    else{
        Log::error(__FILE__, __LINE__, "No pat now!");
        unknown_packet_cnt++;
    }

    if(loss_repeat_ans > 0){
        Log::info(__FILE__, __LINE__, "Repeate a ts pack at time: %f, pts: %d", cur_time_, cur_pts_);
        repeated_pack_cnt++;

        // 写入原始包
        file_out_.write((const char*)buffer, TS_PACKET_LENGTH);

        // 创建重复包，并更新连续计数器（+1）
        u_char repeat_buffer[TS_PACKET_LENGTH];
        memcpy(repeat_buffer, buffer, TS_PACKET_LENGTH);

        // 更新连续计数器（4位，范围0-15）
        u_char cc = (buffer[3] & 0x0F);
        cc = (cc + 1) & 0x0F;  // 循环计数
        repeat_buffer[3] = (repeat_buffer[3] & 0xF0) | cc;

        file_out_.write((const char*)repeat_buffer, TS_PACKET_LENGTH);
        write_packet_cnt +=2;
        return ModTsFileStatus::Running;
    }
    else if(loss_repeat_ans < 0){
        lossed_pack_cnt++;
        Log::info(__FILE__, __LINE__, "Delete a ts pack at time: %f, pts: %d", cur_time_, cur_pts_);
        return ModTsFileStatus::Running;
    }
    else{
        write_packet_cnt++;
        file_out_.write((const char*)buffer, TS_PACKET_LENGTH);
        return ModTsFileStatus::Running;
    }
}

// 获得叠加的跳变
int64_t ModTsFile::getPtsChange(Media media_type,u_int64_t cur_pts){
    int64_t pts_change = 0;
    for(auto iter = pts_pattern_list_.begin(); iter != pts_pattern_list_.end(); iter++){
        FuncPattern* pattern = *iter;
        // 超时的pts pattern删除
        if(cur_time_ > pattern->end_sec){
            Log::info(__FILE__, __LINE__, "At sec %f, pattern overtime, so delete a pattern", cur_time_);
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
            switch (func)
            {
            case PtsFunc::add:
                pts_change += static_cast<int64_t>(base_pts);
                break;
            case PtsFunc::minus:
                pts_change -= static_cast<int64_t>(base_pts);
                break;
            case PtsFunc::sin:
                pts_change += static_cast<int64_t>(sin(cur_time_) * base_pts);
                break;
            case PtsFunc::pulse:
                pts_change += static_cast<int64_t>((sin(cur_time_) >= 0 ? 1 : -1) * base_pts);
                break;
            case PtsFunc::mult:
                // 时间速率调整: new_pts = cur_pts * pts_base, 所以 change = new_pts - cur_pts
                pts_change += static_cast<int64_t>(pattern->pts_base * static_cast<double>(cur_pts)) - static_cast<int64_t>(cur_pts);
                Log::debug(__FILE__, __LINE__, "mult func at %f sec, pts_change: %lld, cur_pts: %llu",
                           cur_time_, pts_change, cur_pts);
                break;
            default:
                break;
            }
        }
    }
    return pts_change;
}


// 修改PTS，增加pts_change值
bool ModTsFile::changePTS(u_char* buffer, int buffer_length, int64_t pts_change){
    (void)buffer_length;  // 未使用但保留以兼容接口
    if(buffer == nullptr){
        return false;
    }
    bool positive = true;
    if(pts_change < 0){
        positive = false;
    }
    uint64_t u_pts_change = 0;
    if(positive){
        u_pts_change = pts_change;
    }
    else{
        u_pts_change = -pts_change;
    }


    uint64_t pts_num = combinePts((char*)buffer);
    Log::debug(__FILE__, __LINE__, "u_pts_change: %llu, pts_num: %llu", u_pts_change, pts_num);

    if(positive){
        pts_num += u_pts_change;
        if(pts_num > 8589934591L){
            pts_num -= 8589934591L;
        }
    }
    else{
        if(u_pts_change > pts_num){
            pts_num += 8589934591L;
        }
        pts_num -= u_pts_change;
    }

    Log::debug(__FILE__, __LINE__, "Final pts_num: %llu", pts_num);
    rewritePts((char*)buffer, pts_num);
    return true;
}

// 修改DTS，增加pts_change值
bool ModTsFile::changeDTS(u_char* buffer, int buffer_length, int64_t dts_change){
    (void)buffer_length;  // 未使用但保留以兼容接口
    if(buffer == nullptr){
        return false;
    }
    bool positive = true;
    if(dts_change < 0){
        positive = false;
    }
    uint64_t u_dts_change = 0;
    if(positive){
        u_dts_change = dts_change;
    }
    else{
        u_dts_change = -dts_change;
    }

    uint64_t dts_num = combinePts((char*)buffer);

    if(positive){
        dts_num += u_dts_change;
        if(dts_num > 8589934591L){
            dts_num -= 8589934591L;
        }
    }
    else{
        if(u_dts_change > dts_num){
            dts_num += 8589934591L;
        }
        dts_num -= u_dts_change;
    }

    rewritePts((char*)buffer, dts_num);  // PTS和DTS格式相同，可以用同一函数覆写
    return true;
}

// 覆写pts
void ModTsFile::rewritePts(char* payload, u_int64_t pts){
    // 清空buffer
    char basic_marker = payload[0];
    basic_marker &= 0b11110001;

    for(int i = 0; i < 5; i++)
        payload[i] = 0;
    // 传入对应的内�?
    payload[0] = (char)((pts >> 29) & 0x0E);
    payload[1] = (char)((pts >> 22) & 0xFF);
    payload[2] = (char)((pts >> 14) & 0xFE);
    payload[3] = (char)((pts >> 7) & 0xFF);
    payload[4] = (char)((pts << 1) & 0xFE);

    // 添加标记�?
    payload[0] |= basic_marker;
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
    if(buffer == nullptr || buffer_length < 9){
        return std::make_pair(0,0);
    }
    int cur_position = 0;
    // 解析pes头部，一共5字节
    PesPacketHeader pes_header;
    memcpy(&pes_header, buffer, 5);

    // 当前包没有pes头
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
        return std::make_pair(0,0);
    }
    else{
        cur_position += 6;
    }

    // 直接解析 OptionalPesHeader（避免内存分配）
    if(buffer_length - cur_position < 3){
        return std::make_pair(0,0);
    }

    u_char byte0 = buffer[cur_position];
    u_char byte1 = buffer[cur_position + 1];
    u_char pts_dts_indicator = (byte1 >> 6) & 0x03;
    u_char marker_bits = (byte0 >> 6) & 0x03;

    // 如果开头不是2，就不是pes adaptation header
    if(marker_bits != 2){
        return std::make_pair(0,0);
    }
    // pts_dts指示表示并不存在pts_dts
    if(pts_dts_indicator == 0b00){
        return std::make_pair(0,0);
    }

    // 有optional header与pts/dts，先移动位置
    cur_position += 3;

    if(pts_dts_indicator == 0b11){
        return std::make_pair(cur_position, cur_position+5);
    }
    // 只有PTS存在
    else if(pts_dts_indicator == 0b10){
        return std::make_pair(cur_position, 0);
    }
    else{
        return std::make_pair(0,0);
    }
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
bool ModTsFile::changeOutputFile(std::ofstream* file_out, std::string& file_name, int slice_count){
    (void)file_out;  // 未使用但保留以兼容接口
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

// 获取指定PID的流类型（内部辅助函数）
uint8_t ModTsFile::getStreamTypeByPid(const int pid){
    for(auto pmt_pair : pmt_pair_list_){
        auto pmt = pmt_pair.second;
        if(pmt != nullptr){
            for(auto program : pmt->PMT_Stream){
                if(pid == program.elementary_PID){
                    return program.stream_type;
                }
            }
        }
    }
    return 0;  // 未找到返回0
}

// 查找pid是否为音频
bool ModTsFile::isAudioPacket(const int pid){
    if(pat_ == nullptr || isPmtPacket(pid)){
        return false;
    }
    uint8_t type = getStreamTypeByPid(pid);
    return (type != 0) && isAudioStream(type);
}

// 查找pid是否为视频
bool ModTsFile::isVideoPacket(const int pid){
    if(pat_ == nullptr || isPmtPacket(pid)){
        return false;
    }
    uint8_t type = getStreamTypeByPid(pid);
    return (type != 0) && isVideoStream(type);
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
        case STREAMTYPE_AVS2_VIDEO:
        case STREAMTYPE_AVS3_VIDEO:
        case STREAMTYPE_HEVC_VIDEO:
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
            Log::debug(__FILE__, __LINE__, "At sec %f, pattern overtime, so delete", cur_time_);
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
                Log::debug(__FILE__, __LINE__, "get a loss func");
                int rate = static_cast<int>(pattern->pts_base) % 100;
                int rand = std::rand() % 100;
                if(rand <= rate || pattern->pts_base >= 100){
                    Log::debug(__FILE__, __LINE__, "delete a packet at pid %d", pid);
                    ans = -1;
                }
                else{
                    ans = 0;
                }
            }
            else if(pattern->pts_func == PtsFunc::repeate){
                Log::debug(__FILE__, __LINE__, "get a repeate func");
                int rate = static_cast<int>(pattern->pts_base) % 100;
                int rand = std::rand() % 100;
                if(rand <= rate || pattern->pts_base >= 100){
                    Log::debug(__FILE__, __LINE__, "repeate a packet at pid %d", pid);
                    ans = 1;
                }
                else{
                    ans = 0;
                }
            }
            else if(pattern->pts_func == PtsFunc::pat_delete){
                Log::debug(__FILE__, __LINE__, "get a pat_delete func");
                if(pid == 0){
                    Log::debug(__FILE__, __LINE__, "delete a PAT at pid %d", pid);
                    ans = -1;
                }
                else{
                    ans = 0;
                }
            }
            else if(pattern->pts_func == PtsFunc::pmt_delete){
                Log::debug(__FILE__, __LINE__, "get a pmt_delete func");
                if(isPmtPacket(pid)){
                    Log::debug(__FILE__, __LINE__, "delete a PMT at pid %d", pid);
                    ans = -1;
                }
                else{
                    ans = 0;
                }
            }
            else{
                Log::debug(__FILE__, __LINE__, "get an unknown func");
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
            Log::debug(__FILE__, __LINE__, "At sec %f, pattern overtime, so delete", cur_time_);
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

// 从头开始写空包（标准空包PID为0x1FFF）
void ModTsFile::writeNullPack(char* payload, int buffer_length){
    static long long null_pack_cnt = 0;
    Log::debug(__FILE__, __LINE__, "Write %lld nullpack", null_pack_cnt++);
    if(payload == nullptr || buffer_length < TS_PACKET_LENGTH){
        return;
    }

    // 写入标准空包头部（PID=0x1FFF）
    payload[0] = 0x47;  // sync_byte
    payload[1] = 0x1F;  // PID高5位（0x1FFF的高5位是0x1F）
    payload[2] = 0xFF;  // PID低8位
    payload[3] = 0x10;  // adaptation_field_control=0x01（只有payload），continuity_counter=0

    // 填充剩余内容为0
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
