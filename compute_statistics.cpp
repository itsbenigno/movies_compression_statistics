#include <string>
#include <array>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace std;


/**
 * Execute <<cmd>> command in bash and get its output
 * @param the command to execute
 * @return the output of the bash command
 */
string exec(const char* cmd) {

    array<char, 128> buffer;
    string result;
    shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get())) {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;

}


/**
 * Given a video, uses ffprobe to obtain the mean bitrate of the video 
 * @param the video
 * @return mean bitrate of video
 */
long int get_video_bitrate(string video_name){
    cout << video_name << endl;
	string command = "ffprobe -v quiet -select_streams v:0 -show_entries stream=bit_rate -of default=noprint_wrappers=1 \""+video_name+"\"";
    string result = exec((command).c_str());
    string delimiter = "=";
    string bit_rate = result.substr(result.find(delimiter)+1, result.length());
	return stoi(bit_rate);
}


/**
 * Given a video and an id, return the name for the compressed video
 * @param the video and the id of the video, n_vid
 * @return the unique name for the compressed video
 */
string get_video_compressed_name(string video, int n_vid){
	filesystem::path p = video;
    string no_extension_name = p.replace_extension();
	string video_compressed_name = no_extension_name + "_compressed_"+to_string(n_vid)+ ".mp4";
	return video_compressed_name;
}


/**
 * Given a video and an id returns the name for the ssim log
 * @param the video and the id of the video
 * @return the unique name for the ssim log
 */ 
string get_ssim_name(string video, int n_vid){
    filesystem::path p = video;
    string no_extension_name = p.replace_extension();
	string ssim_name = no_extension_name + "_"+ to_string(n_vid) + "_ssim.txt";
	return ssim_name;
}


/*
 * Return true if the name of the video contains a .mp4 extension
 */
bool is_video(string video_name){
    
    filesystem::path filePath = video_name;
    if (filePath.extension() == ".mp4")
    {
        return true;
    }
    else
    {
        return false;
    }  
} 


/**
 * Given the ssim file produced by ffmpeg, returns the vector containing the SSIM for each frame of the video
 */ 

vector<double> get_stats(string filename){
    
    vector<double> timeseries;

    ifstream file(filename);
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
            string qualities = line.substr(line.find("All:")+4);
            string ssim = qualities.substr(0, qualities.find(" ("));
            timeseries.push_back(stod(ssim));
        }
        file.close();
    }
    return timeseries;
}


/**
 * 0% compression actually lose something (I think negligible) because we go from VBR to CBR 
 */
vector<vector<double>> compress_videos(){

    vector<vector<double>> video_timeseries;
	double step_pc = 0.1; //step percentage to compress
	double min_pc = 0.1; //min percentage of compression

	//for every file in the directory Videos
    std::string path = "Videos";
    for (const auto & entry : filesystem::directory_iterator(path)){ 
        string video = entry.path(); //path of the video

        if ( is_video(video) ){

    		int video_bit_rate =  get_video_bitrate(video);
    		cout << video << " " <<video_bit_rate << endl;
    		
    		int n_vid = 0; //id of the compressed videovideo
            
            //vector<double> pointwise_summed_timeseries = initialize_empty_vector(len_vector);

    		
            //compress the videos and log the ssim with respect of the original video (with constant bitrate that we created above)
            for (int bit_rate = video_bit_rate; bit_rate >= int(min_pc*video_bit_rate); bit_rate-=video_bit_rate*step_pc){
            	n_vid++; //count a new compression
    			string video_compressed_name = get_video_compressed_name(video, n_vid);
                string ssim_name = get_ssim_name(video, n_vid);

                //compressing
    			system(("ffmpeg -i  \""+ video +"\" -b:v "+to_string(bit_rate)+" -maxrate "+to_string(bit_rate)+" -minrate "+to_string(bit_rate)+" -bufsize "+to_string(bit_rate*2)+" -c:v libx264 \""+ video_compressed_name +"\"").c_str());
                //computing the ssim of the compressed video
    			system(("ffmpeg -i \""+ video_compressed_name + "\" -i \""+video+"\" -lavfi ssim=stats_file=\""+ ssim_name + "\" -f null -").c_str()); 
                //insert ssim into a vector
                get_stats(ssim_name);

    	    }

            //vector<double> regression_compression = pointwise_summed_timeseries / n_vid //(pointwise division, in order to compute regression for 0, i.e. mean)
            //video_stats.push_back(regression_compression); //insert the film timeseries
	   }

    }

    return video_timeseries;
}


/**
 * 
 */
void compute_statistics(vector<vector<double>>){
    //for video in videos

}


/**
 * Compress the videos
 * Compute the statistics 
 */
int main (int argc, char** argv) {
  
  vector<vector<double>> videos_ssim = compress_videos();
  /*compute statistics(videos_ssim)*/


}