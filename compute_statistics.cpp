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
#include <gsl/gsl_vector.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_statistics_double.h>

using namespace std;

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
 * Write the data vector in filename
 * @param the filename to write in (use .csv extension), the vector containing the timeseries
 */
void write_timeseries(const vector<double> &data, const string &filename)
{
  ofstream file(filename);

  if (!file.is_open())
  {
    cerr << "Could not open file: " << filename << endl;
    return;
  }

  for (double value : data)
  {
    file << value << "\n";
  }

  file.close();
}

/**
 * Given a video, uses ffprobe to obtain the bitrate of the video
 * note: the bitrate of a vbr video is not always the same since its a vbr
 * @param the video path
 * @return bitrate of video
 */
long int get_video_bitrate(const string &video_name)
{
  cout << video_name << endl;
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
    cerr << "Error parsing bitrate: Invalid argument." << endl;
    return -1;
  }
  catch (const out_of_range &e)
  {
    cerr << "Error parsing bitrate: Value out of range." << endl;
    return -1;
  }
}

/**
 * Given a video and an id, return the name for the compressed video
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
 * Given a video and an id returns the name for the ssim log
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
 * Given the path of a video, return true if it has .mp4 as extension
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
 * Retrieve the order of the series contained in filename
 * @param the filename (in .csv extensioni)
 * @return the vector containing p,d,q
 */
vector<int> get_arima_order(const std::string &filename)
{
  std::ifstream file(filename);
  std::string line;
  std::vector<int> values;

  if (!file.is_open())
  {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return values;
  }

  if (!std::getline(file, line))
  {
    std::cerr << "Failed to read values line from file: " << filename << std::endl;
    return values;
  }

  std::string value;
  std::istringstream line_stream(line);

  for (int i = 0; i < 3; i++)
  {
    if (!std::getline(line_stream, value, ','))
    {
      std::cerr << "Failed to read value from line: " << line << std::endl;
      return values;
    }

    try
    {
      values.push_back(std::stoi(value));
    }
    catch (const std::invalid_argument &e)
    {
      std::cerr << "Failed to convert value to integer: " << value << std::endl;
      return values;
    }
    catch (const std::out_of_range &e)
    {
      std::cerr << "Value is out of range: " << value << std::endl;
      return values;
    }
  }

  return values;
}

/**
 * Extract the SSIM from the file produced by ffmpeg
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
 * We represent the element wise mean of two vectors as
 * {2,4},{4,8} => {((2+2)/2), ((4+8)/2)} => {3,6}
 * @param a vector of vectors (each vector contains the ssim timeseries at a certain compression)
 * @return element wise mean as described above
 */
vector<double> mean_elt_wise(const vector<vector<double>> vec)
{

  vector<double> result(vec[0].size(), 0.0); // create a vector with the same size as the inner vectors, initialized to 0.0

  // iterate over the inner vectors
  for (const auto &inner : vec)
  {
    // add the elements of the inner vector element-wise to the result vector
    for (size_t i = 0; i < inner.size(); ++i)
    {
      result[i] += inner[i];
    }
  }

  // divide each element of the result vector by the number of inner vectors
  for (auto &elt : result)
  {
    elt /= vec.size();
  }

  return result;
}

/**
 * Take the videos in "Videos" folder.
 * For each video: apply a certain pct of compression, and compute the ssim
 * Then do the mean and obtain a linear regression model as a time series
 * note: 0% compression actually lose something (I think negligible) because we go from VBR to CBR
 * @param void
 * @return a vector containing for each video the ssim model timeseries
 */
vector<vector<double>> compute_ssim_model()
{

  vector<vector<double>> video_timeseries;
  double step_pc = 0.3; // step percentage to compress
  double min_pc = 0.1;  // min percentage of compression

  // for every file in the directory Videos
  string path = "Videos";
  for (const auto &entry : filesystem::directory_iterator(path))
  {
    string video = entry.path(); // path of the video

    if (is_video(video))
    {

      int video_bit_rate = get_video_bitrate(video);
      cout << video << " " << video_bit_rate << endl;

      int n_vid = 0; // id of the compressed video
      vector<vector<double>> video_compressions;

      // compress the videos and log the ssim with respect of the original video (with constant bitrate that we created above)
      for (int bit_rate = video_bit_rate; bit_rate >= int(min_pc * video_bit_rate); bit_rate -= video_bit_rate * step_pc)
      {
        n_vid++; // count a new compression
        string video_compressed_name = get_video_compressed_name(video, n_vid);
        string ssim_name = get_ssim_name(video, n_vid);

        // compressing
        system(("ffmpeg -i  \"" + video + "\" -b:v " + to_string(bit_rate) + " -maxrate " + to_string(bit_rate) + " -minrate " + to_string(bit_rate) + " -bufsize " + to_string(bit_rate * 2) + " -c:v libx264 \"" + video_compressed_name + "\"").c_str());
        // computing the ssim of the compressed video
        system(("ffmpeg -i \"" + video_compressed_name + "\" -i \"" + video + "\" -lavfi ssim=stats_file=\"" + ssim_name + "\" -f null -").c_str());
        // saving it into a vector
        video_compressions.push_back(get_stats(ssim_name));
      }
      // the actual timeseries is the mean of the timeseries at each compression
      video_timeseries.push_back(mean_elt_wise(video_compressions));
    }
  }

  return video_timeseries;
}
/**
 * Write the arima order and the moments ranges into txt files
 * @param the moments range and the max arima order
 */
void save_videos_stats(const vector<int> &order, const vector<vector<double>> &moments)
{
  ofstream moments_file("moments_range.txt");
  if (moments_file.is_open())
  {
    for (const auto &item : moments)
    {
      for (const auto &value : item)
      {
        moments_file << value << " ";
      }
      moments_file << endl;
    }
    moments_file.close();
  }
  else
  {
    cerr << "Error opening file" << endl;
  }
  ofstream order_file("max_arima_order.txt");
  if (order_file.is_open())
  {
    for (const auto &value : order)
    {
      order_file << value << " ";
    }
    order_file.close();
  }
  else
  {
    cerr << "Error opening file" << endl;
  }
}

/**
 * Using the GSL library computes the mean, variance, and lag1 autocorellation
 * for each timeseries, and also the ARIMA order of the series.
 * Keeps the range of the moments and the maximum order and saves them into a file
 * @param a vector containing the ssim timeseries for each video
 */
bool compute_statistics(const vector<vector<double>> &input)
{

  vector<vector<double>> moments_range;

  vector<int> max_arima_order{0, 0, 0};
  vector<double> mean_range = {numeric_limits<double>::max(), numeric_limits<double>::lowest()};
  vector<double> variance_range = {numeric_limits<double>::max(), numeric_limits<double>::lowest()};
  vector<double> autocorrelation_range = {numeric_limits<double>::max(), numeric_limits<double>::lowest()};

  for (const auto &timeseries : input)
  {
    // write timeseries to csv
    string timeseries_filename = "timeseries.csv";
    write_timeseries(timeseries, timeseries_filename);

    // get the order of the model
    string max_search_p = "10"; // maximum p for the model
    string max_search_d = "3";
    string max_search_q = "10";
    string command = "python3 model_calculator.py \"" + timeseries_filename + "\" " + max_search_p + " " + max_search_d + " " + max_search_q;
    try
    {
      exec(command.c_str());
    }
    catch (const runtime_error &e)
    {
      cerr << "Error executing command: " << e.what() << endl;
      return false;
    }
    // if the vector is empy return an error
    string arima_order_filename = "best_ARIMA_order.csv";
    vector<int> arima_order = get_arima_order(arima_order_filename);
    if (arima_order.empty())
    {
      return false;
    }

    for (int i = 0; i < max_arima_order.size(); i++)
    {
      max_arima_order[i] = max(max_arima_order[i], arima_order[i]);
    }

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

    // update the ranges of the moments
    mean_range[0] = min(mean_range[0], mean);
    mean_range[1] = max(mean_range[1], mean);

    variance_range[0] = min(variance_range[0], variance);
    variance_range[1] = max(variance_range[1], variance);

    autocorrelation_range[0] = min(autocorrelation_range[0], autocorr);
    autocorrelation_range[1] = max(autocorrelation_range[1], autocorr);

    // Free the gsl_vector
    gsl_vector_free(v);
  }

  moments_range.push_back(mean_range);
  moments_range.push_back(variance_range);
  moments_range.push_back(autocorrelation_range);

  save_videos_stats(max_arima_order, moments_range);

  return true;
}

/**
 * Given a folder "Videos" containing various videos
 * For each video:
 * - Compute the timeseries of the ssim for certain compression
 * - Compute the mean (i.e. regression for 0) of the ssim
 * - Compute the moments of the mean of the ssim timeseries
 * - Write them on a file
 */
int main(int argc, char **argv)
{

  vector<vector<double>> videos_ssim = compute_ssim_model();
  if (compute_statistics(videos_ssim))
  {
    return 0;
  }
  else
  {
    return 1;
  }
}