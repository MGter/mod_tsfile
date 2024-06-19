# -*- coding: utf-8 -*-
import json
import os

# task结构
class TaskParam:
    # task param
    id = 0
    input_file = ""
    output_file = ""
    start_sec = -1
    end_sec = -1
    # pattern param
    pattern_start_sec = -1
    pattern_end_sec = -1
    pattern_media = "all"
    pattern_func = "add"
    pattern_pts_base = 0
    

def remove_suffix(text, suffix):
    if text.endswith(suffix):
        return text[: -len(suffix)]
    return text


def name_parse(task_param):
    input_file = task_param.input_file
    pattern_func = task_param.pattern_func
    pattern_media = task_param.pattern_media
    start_sec = task_param.pattern_start_sec
    end_sec = task_param.pattern_end_sec
    times = task_param.pattern_pts_base
    pts_sec = task_param.pattern_pts_base / 90000
    pure_name = remove_suffix(input_file, ".ts")
    output_name = ""
    # 删除pat的名字
    if(pattern_func == "pmt_delete"):
        output_name = pure_name + "_" + pattern_media + "_from_sec" + str(start_sec) + "_to_sec" + str(end_sec) + "_" + pattern_func + ".ts"
    elif(pattern_func == "pat_delete"):
        output_name = pure_name + "_from_sec" + str(start_sec) + "_to_sec" + str(end_sec) + "_" + pattern_func + ".ts"
    elif(pattern_func == "null"):
        output_name = pure_name + "_" + pattern_media + "_from_sec" + str(start_sec) + "_to_sec" + str(end_sec) + "_" + pattern_func + "pack.ts"
    elif(pattern_func == "mult"):
        output_name = pure_name + "_" + pattern_media + "_from_sec_" + str(start_sec) + "_to_sec_" + str(end_sec) + "_" + pattern_func + "_"+ "{:.1f}".format(times) + "_times.ts"
    elif(pattern_func == "loss" or pattern_media == "repeate"):
        output_name = pure_name + "_" + pattern_media + "_from_sec_" + str(start_sec) + "_to_sec_" + str(end_sec) + "_" + pattern_func + "_"+ "{:.1f}".format(times) + "_percent.ts"
    else:
        output_name = pure_name + "_" + pattern_media + "_media_from_sec_" + str(start_sec) + "_to_sec_" + str(end_sec) + "_" + pattern_func + "_"+ str(pts_sec) + "_sec_pts.ts"
    
    return output_name

class JsonMaker:
    def __init__(self):
        self.task_array = []
        
    def json_add(self, task_param):
        # parse task
        task = {}
        task["id"] = task_param.id
        task["input_file"] = task_param.input_file
        task["output_file"] = task_param.output_file
        task["pattern"] = []
        
        # parse pattern
        pattern = {}
        pattern["start_sec"] = task_param.pattern_start_sec
        pattern["end_sec"] = task_param.pattern_end_sec
        pattern["media"] = task_param.pattern_media
        pattern["func"] = task_param.pattern_func
        pattern["pts_base"] = task_param.pattern_pts_base
        task["pattern"].append(pattern)
        
        patten2 = {}
        patten2["start_sec"] = task_param.start_sec
        patten2["end_sec"] = task_param.end_sec
        patten2["media"] = "all"
        patten2["func"] = "cut"
        patten2["pts_base"] = 0
        task["pattern"].append(patten2)
        
        # add into task_list
        self.task_array.append(task)
        
    def json_make(self, output_json_file):
        ans = {}
        ans["task_array"] = self.task_array
        with open(output_json_file, "w") as file:
            json.dump(ans, file, indent=4)
            print("Success to make the output conf at " + output_json_file)
            
            
if __name__ == "__main__":
    output_json_name = "output.json"
    input_file = "CBR_5M_H265_HEVC_AAC_LC_HD_25f_420.ts"
    
    param_list = []
    
    
    # pts change
    media_list = ["all", "audio", "video"]
    func_list = ["add", "minus", "sin", "pulse"]
    pts_base_list = [180000, 540000]
    for media in media_list:
        for func in func_list:
            for pts_base in pts_base_list:
                pattern = TaskParam()
                pattern.input_file = input_file
                pattern.start_sec = -1
                pattern.end_sec = 180
                pattern.pattern_start_sec = 30
                pattern.pattern_end_sec = 90
                pattern.pattern_media = media
                pattern.pattern_func = func
                pattern.pattern_pts_base = pts_base
                output_file = name_parse(pattern)
                pattern.output_file = output_file
                param_list.append(pattern)
    
    # time_rate_change
    media_list = ["all"]
    func_list = ["mult"]
    pts_base_list = [0.6, 0.8, 1.2, 1.4]
    for media in media_list:
        for func in func_list:
            for pts_base in pts_base_list:
                pattern = TaskParam()
                pattern.input_file = input_file
                pattern.start_sec = -1
                pattern.end_sec = 180
                pattern.pattern_start_sec = 30
                pattern.pattern_end_sec = 90
                pattern.pattern_media = media
                pattern.pattern_func = func
                pattern.pattern_pts_base = pts_base
                output_file = name_parse(pattern)
                pattern.output_file = output_file
                param_list.append(pattern)

    '''
    # time_rate_change
    media_list = ["all"]
    func_list = ["null"]
    pts_base_list = [5]
    for media in media_list:
        for func in func_list:
            for pts_base in pts_base_list:
                pattern = TaskParam()
                pattern.input_file = input_file
                pattern.start_sec = -1
                pattern.end_sec = 180
                pattern.pattern_start_sec = 30
                pattern.pattern_end_sec = 90
                pattern.pattern_media = media
                pattern.pattern_func = func
                pattern.pattern_pts_base = pts_base
                output_file = name_parse(pattern)
                pattern.output_file = output_file
                param_list.append(pattern)
    '''
    '''
    # loss
    media_list = ["all", "audio", "video"]
    func_list = ["loss", "repeate"]
    pts_base_list = [20, 50]
    for media in media_list:
        for func in func_list:
            for pts_base in pts_base_list:
                pattern = TaskParam()
                pattern.input_file = input_file
                pattern.start_sec = -1
                pattern.end_sec = 180
                pattern.pattern_start_sec = 30
                pattern.pattern_end_sec = 90
                pattern.pattern_media = media
                pattern.pattern_func = func
                pattern.pattern_pts_base = pts_base
                output_file = name_parse(pattern)
                pattern.output_file = output_file
                param_list.append(pattern)

    # 删除pmt，可以选择删除多少
    media_list = ["all", "audio", "video"]
    func_list = ["pmt_delete"]
    pts_base_list = [0]
    for media in media_list:
        for func in func_list:
            for pts_base in pts_base_list:
                pattern = TaskParam()
                pattern.input_file = input_file
                pattern.start_sec = -1
                pattern.end_sec = 180
                pattern.pattern_start_sec = 30
                pattern.pattern_end_sec = 90
                pattern.pattern_media = media
                pattern.pattern_func = func
                pattern.pattern_pts_base = pts_base
                output_file = name_parse(pattern)
                pattern.output_file = output_file
                param_list.append(pattern)
    
    # 删除pat，可以选择删除多少   
    media_list = ["all"]
    func_list = ["pat_delete"]
    pts_base_list = [0]
    for media in media_list:
        for func in func_list:
            for pts_base in pts_base_list:
                pattern = TaskParam()
                pattern.input_file = input_file
                pattern.start_sec = -1
                pattern.end_sec = 180
                pattern.pattern_start_sec = 30
                pattern.pattern_end_sec = 90
                pattern.pattern_media = media
                pattern.pattern_func = func
                pattern.pattern_pts_base = pts_base
                output_file = name_parse(pattern)
                pattern.output_file = output_file
                param_list.append(pattern)
'''
    # arrange the id
    for i in range(len(param_list)):
        param_list[i].id = i
        

    # 将所有的Input都转化为output_origin的格式，或者说转化为srt字幕
    # 然后改写input的名称
    for param in param_list:
        os.system("echo \"\" > input.srt")
        os.system("echo \"1\" >> input.srt")
        os.system("echo \"00:00:00,000 --> 00:03:00,000\" >> input.srt")
        split_length = 40

        splits = [ param.output_file[i:i+split_length] for i in range(0, len(param.output_file), split_length)]
        for split in splits:
            command = "echo \"" + split + "\" >>    input.srt"
            os.system(command)
        param.input_file = param.output_file + "_subtitle.ts"
        os.system("ffmpeg -i juemishiming.mp4 -g 25 -r 25 -s 1920x1080 -c:v libx265 -b:v 4500k -minrate 4500k -maxrate 4500k -c:a aac -b:a 500k -bufsize 5000k -nal-hrd cbr -muxrate 5000k -vf \"subtitles=input.srt:force_style=force_style='Alignment=2,MarginV=240'\" -streamid 0:0x100 -streamid 1:0x101 " + str(param.input_file))


    # trying to make the json
    maker = JsonMaker()
    for param in param_list:
        maker.json_add(param)
    maker.json_make(output_json_name)
    print("Success to make the output json: " + output_json_name)


