#include <iostream>
#include <string>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <aubio/aubio.h>
#include <vector>

namespace {
	std::vector<std::vector<float>> features;
}

bool	getBeat(const std::string &file) {
	aubio_tempo_t 		*tempo;
  	uint_t 				win_size 		= 1024;
  	uint_t 				hop_size 		= win_size / 4;
  	uint_t 				n_frames 		= 0;
  	uint_t				read 			= 0;
	aubio_source_t		*aubio_source 	= NULL;
	uint_t				samplerate		= 0;

	// Create a new source object from the file given as parameter
	// If samplerate is set to 0, the sample rate of the original file is used
	// hop_size is the size of the blocks to read from the source
	if (!(aubio_source = new_aubio_source (const_cast<char *>(file.c_str()), samplerate, hop_size))) {
		std::cerr << "new_aubio_source failed" << std::endl;
		aubio_cleanup();
		return false;
	}
	
	// Get the samplerate from the file
	if ((samplerate = aubio_source_get_samplerate(aubio_source)) == 0) {
		std::cerr << "aubio_source_get_samplerate failed" << std::endl;
		del_aubio_source(aubio_source);
		aubio_cleanup();
		return false;
	}
	
	// Create two vectors
	// One input buffer and one for the output position
	fvec_t *in 	= new_fvec(hop_size);
  	fvec_t *out = new_fvec(2);

  	// Create the tempo object
  	// The "default" method is used, beattracking, win_size is the length of FFT
  	// hop_size is the number of frames between two consecutive runs
  	std::string tempo_method = "default";
	if (!(tempo = new_aubio_tempo(const_cast<char *>(tempo_method.c_str()), win_size, hop_size, samplerate))) {
		std::cerr << "new_aubio_tempo failed" << std::endl;
		del_aubio_source(aubio_source);
		aubio_cleanup();
		return false;		
	}

	// Find the tempo
	do {
		// Put data in the input buffer
		aubio_source_do(aubio_source, in, &read);
		// tempo detection
		aubio_tempo_do(tempo, in, out);
		// manipulate the beat
		if (out->data[0] != 0) {
      		std::cout << "beat at " << aubio_tempo_get_last_ms(tempo) << "ms, " << aubio_tempo_get_last_s(tempo) << "s, frame " << aubio_tempo_get_last(tempo) << ", " << aubio_tempo_get_bpm(tempo) << "bpm with confidence " << aubio_tempo_get_confidence(tempo) << std::endl;
      	}
    	n_frames += read;
  	} while (read == hop_size);

  	std::cout << "read " << n_frames * 1. / samplerate << "s, " << n_frames << " frames at " << samplerate << "Hz ("<< n_frames / hop_size << " blocks) from " << file << std::endl;

	// clean up memory
  	del_aubio_tempo(tempo);
  	del_fvec(in);
  	del_fvec(out);
  	del_aubio_source(aubio_source);
  	return true;
}

void	getFeatures(const std::string &file) {
	getBeat(file);
}


std::string	GetFileExtension(std::string file) {
	std::string::size_type idx = file.rfind('.');
	if (idx != std::string::npos)
		return file.substr(idx + 1);;
	return std::string("");
}

int 	ExecCommand(std::string const &cmd) {
	int		status;
	pid_t	pid;
	
	if ((pid = fork()) < 0) {
			std::cerr << "Fork failed" << std::endl;
		} else if (pid == 0) {
			std::system(cmd.c_str());
			exit(1);
		} else {
			if (waitpid(pid, &status, 0) > 0) {
				if (WIFEXITED(status)) 
					return WIFEXITED(status);
			}
	}
	return 1;
}

/**
 * Normalize the dataset using ffmpeg
 * Each song is converted into wave (PCM) making it mono with a bit rate of 128kbps
 * and a sampling frequency of 44.1kHz
 * Trim the song to recover 60 seconds that are more likely to contain the chorus.
 * NB: We have defined these 60 seconds to start from 00:45:00 to 01:45:00.
 * Ugly implementation
 * @param file to process and directory in which we should put the processed sound.
 */
void	ProcessSound(std::string const &file, std::string const &directory) {
	std::string extension = GetFileExtension(file);
	if (extension == "mp3") {
		std::string filename = file.substr(0, file.length() - (extension.length() + 1));
		std::string cmd = "ffmpeg -i \"" + directory + "/" + file 
						+ "\" -ar 44100 -ac 1 -codec:a libmp3lame -b:a 128k \"" 
						+ directory + "/" + filename + ".wav\"";
		ExecCommand(cmd);
		cmd = "ffmpeg -i \"" + directory + "/" + filename + ".wav" 
						+ "\" -ss 00:00:45 -t 00:01:00 -acodec copy \"" + directory 
						+ "/" + filename + "_60_seconds.wav\"";
		ExecCommand(cmd);
	}
}


bool	normalize_dataset(std::string const &directory) {
	DIR		*dir;
	dirent	*pdir;

	if ((dir = opendir(directory.c_str())) == NULL){
		std::cerr << "Open directory " << directory << " failed" << std::endl;
		return false;
	}

	while ((pdir = readdir(dir))) {
		if (pdir->d_type == DT_REG) {
			 ProcessSound(pdir->d_name, directory);
		}
	}
	closedir(dir);
	return true;
}

int main(int ac, char **av){
	getFeatures("nflikeabird.wav");/*
	if (ac < 2) {
		std::cout << "Usage:" << av[0] << " <training_directory>" << std::endl;
		return 1;
	}
	normalize_dataset(av[1]);*/
    return 0;
}