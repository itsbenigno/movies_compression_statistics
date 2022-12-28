#include <string>
#include <array>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <filesystem>

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
	string command = "ffprobe -v quiet -select_streams v:0 -show_entries stream=bit_rate -of default=noprint_wrappers=1 ";
    string result = exec((command+video_name).c_str());
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
	string delimiter = ".";
    string no_extension_name = video.substr(0, video.find(delimiter));
	string video_compressed_name = no_extension_name + "_compressed_"+to_string(n_vid)+ ".mp4";
	return video_compressed_name;
}


/**
 * Given a video and an id returns the name for the ssim log
 * @param the video and the id of the video
 * @return the unique name for the ssim log
 */ 
string get_ssim_name(string video, int n_vid){
	string delimiter = ".";
    string no_extension_name = video.substr(0, video.find(delimiter));
	string ssim_name = no_extension_name + to_string(n_vid) + "_ssim.txt";
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
 * 

vector<double> get_stats(){
    for righe in testo:
    OKKIO A EVENTUALI +1
        cerca il token "All:"
        cerca il token " " nella sottostringa che parte dal token "All"
        converti in intero la sottostringa rimanente
}*/

/**
 * require video names to be without spaces, at least for now
 */
void compress_videos(){

	double step_pc = 0.2; //step percentage to compress
	double min_pc = 0.1; //min percentage of compression

	//for every file in the directory Videos
    std::string path = "Videos";
    for (const auto & entry : filesystem::directory_iterator(path)){ 
        string video = entry.path(); //path of the video

        if ( is_video(video) ){

		int video_bit_rate =  get_video_bitrate(video);
		cout << video << " " <<video_bit_rate << endl;
		
		int n_vid = 0; //id of the compressed videovideo

		//rename variable bitrate video to tmp.mp4
		if (std::rename(video.c_str(), "Videos/tmp.mp4")) {
	        std::perror("Error renaming");
    	}

    	//convert original video to a constant bitrate video, retaining the same quality
		system(("ffmpeg -i Videos/tmp.mp4 -b:v "+to_string(video_bit_rate)+" -maxrate "+to_string(video_bit_rate)+" -minrate "+to_string(video_bit_rate)+" -bufsize "+to_string(video_bit_rate*2)+" -c:v libx264 "+ video).c_str());
		video_bit_rate-=video_bit_rate*step_pc;
		system("rm Videos/tmp.mp4");

		//compress the videos and log the ssim with respect of the original video (with constant bitrate that we created above)
		for (int bit_rate = video_bit_rate; bit_rate >= int(min_pc*video_bit_rate); bit_rate-=video_bit_rate*step_pc){
        	n_vid++;
			
			system(("ffmpeg -i "+ video +" -b:v "+to_string(bit_rate)+" -maxrate "+to_string(bit_rate)+" -minrate "+to_string(bit_rate)+" -bufsize "+to_string(bit_rate*2)+" -c:v libx264 "+ get_video_compressed_name(video, n_vid)).c_str());

			system(("ffmpeg -i "+ get_video_compressed_name(video, n_vid) + " -i "+video+" -lavfi ssim=stats_file="+ get_ssim_name(video, n_vid) + " -f null -").c_str());

	       }
	   }
    }
}


/**
 * Compress the videos
 * Compute the statistics 
 */
int main (int argc, char** argv) {
  
  compress_videos();
  /*compute statistics*/


}