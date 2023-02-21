#include <string>
#include <array>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <system_error>
#include <limits>
#include <map>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_statistics_double.h>

using namespace std;

namespace statistics_constants
{
  const string VIDEO_FOLDER_NAME = "Videos"; // where the videos from which compute statistics are stored
  double COMPRESSION_STEP = 0.1;             // step percentage to compress
  double MIN_COMPRESSION = 0.1;              // min percentage of compression
  double MAX_COMPRESSION = 0.9;              // min percentage of compression
  const string OUTPUT_FILENAME = "movie_statistics.csv";
}

/**
 * Execute <<cmd>> command in bash and get its output
 * @param the command to execute
 * @return the output of the bash command
 */
string exec(const char *cmd)
{
  unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe)
  {
    throw runtime_error("Failed to execute command.");
  }

  stringstream result;
  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr)
  {
    result << buffer;
  }

  return result.str();
}

/**
 * Given a video, uses ffprobe to obtain the bitrate of the video
 * note: the bitrate of a vbr video is difficult to estimate
 * @param the video path
 * @return bitrate of video
 */
long int get_video_bitrate(const string &video_name)
{
  filesystem::path video_path(video_name);
  string command = "ffprobe -v quiet -select_streams v:0 -show_entries stream=bit_rate -of default=noprint_wrappers=1 \"" + video_path.string() + "\"";
  string result;
  try
  {
    result = exec(command.c_str());
  }
  catch (const runtime_error &e)
  {
    cerr << "Error executing command: " << e.what() << endl;
    return -1;
  }

  string delimiter = "=";
  size_t pos = result.find(delimiter);
  if (pos == string::npos)
  {
    cerr << "Error parsing bitrate: Delimiter not found." << endl;
    return -1;
  }

  string bit_rate = result.substr(pos + 1, result.length());
  try
  {
    return stol(bit_rate);
  }
  catch (const invalid_argument &e)
  {
    cerr << "Error parsing bitrate: not a number." << endl;
    return -1;
  }
  catch (const out_of_range &e)
  {
    cerr << "Error parsing bitrate: Value out of range." << endl;
    return -1;
  }
}

/**
 * Given a video path and an id, return the name for the compressed video
 * @param the video and the id of the video (n_vid)
 * @return the unique name for the compressed video
 */
string get_video_compressed_name(const string &video, int n_vid)
{
  filesystem::path input_path(video);
  string output_stem = input_path.stem();
  output_stem.append("_compressed_");
  output_stem.append(to_string(n_vid));
  filesystem::path output_path = input_path.parent_path() / (output_stem + ".mp4");
  return output_path.string();
}

/**
 * Given a video path and an id returns the name for the ssim log
 * @param the video and the id of the video
 * @return the unique name for the ssim log
 */
string get_ssim_name(const string &video, int n_vid)
{
  filesystem::path input_path(video);
  string output_stem = input_path.stem();
  output_stem.append("_");
  output_stem.append(to_string(n_vid));
  output_stem.append("_ssim");
  filesystem::path output_path = input_path.parent_path() / (output_stem + ".txt");
  return output_path.string();
}

/**
 * Given the path of a video, return true if it is a .mp4 video
 * @param the path of a video
 * @return true if the video_path is a file with .mp4 extension
 */
bool is_video(string video_name)
{
  filesystem::path filePath = video_name;
  string extension = filePath.extension().string();
  transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
  if (extension == ".mp4")
  {
    if (filesystem::exists(filePath))
    {
      return true;
    }
  }
  return false;
}

/**
 * Extract the SSIM timeseries from the file produced by ffmpeg
 * Each line represents a frame
 * e.g.
 * n:4 Y:0.950110 U:0.988194 V:0.984612 All:0.962208 (14.225971)
 * the ssim is 0.962208 in this case
 * @param filename containing the SSIM file produced by ffmpeg
 * @return the vector that contains the SSIM for each frame (information contained in filename)
 */
vector<double> get_stats(string filename)
{

  vector<double> timeseries;

  ifstream file(filename);
  if (file.is_open())
  {
    string line;
    while (getline(file, line))
    {
      string qualities = line.substr(line.find("All:") + 4);
      string ssim = qualities.substr(0, qualities.find(" ("));
      timeseries.push_back(stod(ssim));
    }
    file.close();
  }
  return timeseries;
}

/**
 * Take the videos in "Videos" folder.
 * For each video:
 *  For each compression:
 *    compute the compressed video ssim timeseries
 * note: 0% compression actually lose something (I think negligible) because we go from VBR to CBR
 * @param void
 * @return a vector containing for each video the ssim model timeseries
 */
map<string, map<double, vector<double>>> compute_videos_ssim()
{
  map<string, map<double, vector<double>>> video_compressions_timeseries;

  // for every file in the directory Videos
  for (const auto &entry : filesystem::directory_iterator(statistics_constants::VIDEO_FOLDER_NAME))
  {
    string video_path = entry.path(); // path of the video

    if (is_video(video_path))
    {
      string video_name = entry.path().stem(); // name of the video
      map<double, vector<double>> compressions_timeseries;

      int video_bit_rate = get_video_bitrate(video_path);

      int n_vid = 0; // id of the compressed video

      double compression = 0;

      // compress the videos and log the ssim with respect of the original video
      for (double compression = statistics_constants::MIN_COMPRESSION; compression <= statistics_constants::MAX_COMPRESSION; compression += statistics_constants::COMPRESSION_STEP)
      {
        cout << "Starting compression " << compression << " for " << video_name << endl;

        n_vid++;

        int bit_rate = video_bit_rate * (1 - compression);

        string video_compressed_name = get_video_compressed_name(video_path, n_vid);
        string ssim_name = get_ssim_name(video_path, n_vid);

        // compressing
        system(("ffmpeg -v quiet -i  \"" + video_path + "\" -b:v " + to_string(bit_rate) + " -maxrate " + to_string(bit_rate) + " -minrate " + to_string(bit_rate) + " -bufsize " + to_string(bit_rate * 2) + " -c:v libx264 \"" + video_compressed_name + "\"").c_str());

        // computing the ssim of the compressed video
        system(("ffmpeg -v quiet -i \"" + video_compressed_name + "\" -i \"" + video_path + "\" -lavfi ssim=stats_file=\"" + ssim_name + "\" -f null -").c_str());

        // saving the timeseries for this particular compression
        compressions_timeseries[compression] = get_stats(ssim_name);
      }
      video_compressions_timeseries[video_name] = compressions_timeseries;
    }
  }

  return video_compressions_timeseries;
}

/**
 * Using the GSL library compute the mean, variance, and lag1 autocorellation
 * for each timeseries. Then write the moments of each film (for each compression)
 * in a csv fil
 * @param a vector containing the ssim timeseries for each video
 * @return true if there are no errors in the execution
 */
bool compute_statistics(const map<string, map<double, vector<double>>> &input_timeseries)
{
  std::ofstream output_file(statistics_constants::OUTPUT_FILENAME);

  // Write the header row
  output_file << "Title,";
  vector<string> strings = {"mean", "variance", "lag1autocorrelation"};
  int num_steps = (statistics_constants::MAX_COMPRESSION - statistics_constants::MIN_COMPRESSION) / statistics_constants::COMPRESSION_STEP + 1; // size of the compressions steps
  vector<double> compressions(num_steps);
  for (int i = 0; i < num_steps; i++)
  {
    compressions[i] = statistics_constants::MIN_COMPRESSION + i * statistics_constants::COMPRESSION_STEP;
  }
  for (auto cmp : compressions)
  {
    for (auto str : strings)
    {
      output_file << str << "-" << cmp << ",";
    }
  }
  output_file << endl;

  for (const auto &video : input_timeseries)
  {
    string video_name = video.first;

    output_file << video_name << ",";
    for (const auto &compression_timeseries : video.second)
    {
      double compression = compression_timeseries.first;
      std::vector<double> timeseries = compression_timeseries.second;

      // Convert the vector to a gsl_vector
      gsl_vector *v = gsl_vector_alloc(timeseries.size());
      for (size_t i = 0; i < timeseries.size(); i++)
      {
        gsl_vector_set(v, i, timeseries[i]);
      }

      // Compute the mean of the timeseries
      double mean = gsl_stats_mean(v->data, v->stride, v->size);

      // Compute the variance of the timeseries
      double variance = gsl_stats_variance(v->data, v->stride, v->size);

      // Compute the autocorrelation of lag 1 of the timeseries
      double autocorr = gsl_stats_lag1_autocorrelation(v->data, v->stride, v->size);

      output_file << mean << "," << variance << "," << autocorr << ",";

      // Free the gsl_vector
      gsl_vector_free(v);
    }
    output_file << endl;
  }

  output_file.close();

  return true;
}

/**
 * Given a folder containing videos
 * For each video:
 * - Compute the timeseries of the ssim for certain compression
 * - Compute the moments of the timeseries
 * - Write them on a csv where films are row, and moments are column
 *   e.g. film1: mean10%, variance10%, ..., mean90%, variance90%
 */
int main(int argc, char **argv)
{
  map<string, map<double, vector<double>>> videos_ssim = compute_videos_ssim();

  return compute_statistics(videos_ssim) ? 0 : 1;
}