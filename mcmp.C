/* mcmp compares multiple files returning a list of duplicates.
 * -t: the list will include all files with the same contents except the oldest
*       such file
*       By default, the first file in the list will never be printed, so if the
* 	ouptut is used as a list of files to remove, the first is retained.
*	basically, -t retains the oldest dup while the default is to retain the
*       first file
*/
#include <iostream>
#include <fstream>
#include <string>
#ifndef TARGET_OS_MAC
#include <cstring>
#endif
#include <unistd.h>
#include <list>
#include <sys/stat.h>
using namespace std;
#define USAGE  "mcmp -t -d f1 f2 f3 f4\n\
\t-d debug\n\
\t-t keep oldest file that matches f1"
/* Class CandidateFile 
 * represents a file that is checked for a match
 * mod_time is used to find the oldest file.  This only makes sense after all
 * files have been examined. One could sort first.
 */
class CandidateFile {
	public: std::string fileName;
	std::fstream thisStream;
	int size;
	int mod_time;
	bool candidate ;
	CandidateFile(std::string str) {
		this->candidate=true;
		this->fileName= str;
		struct stat statbuf;
		int rc = stat(this->fileName.c_str(),&statbuf);
		if ( rc == 0 ) {
			this->size=statbuf.st_size;
			this->mod_time=statbuf.st_mtime;
			this->thisStream.open(this->fileName);
		} else {
			std::cerr << "Stat failed on " << this->fileName << std::endl;
			this->candidate=false;
		}
	}
	void closeFile() {
		if ( this->thisStream.is_open() ) {
			this->thisStream.close();
		}
	
	}
	~CandidateFile() {
		closeFile();
	}
};
const char *optStr="Df";
int main(int argc,char **argv)  {
	int opt;
	bool debug=false;
	bool useFirst=true;
	int bufSize=1000;
	int fileCnt = 0;
	while ((opt = getopt(argc, argv, "dths;")) != -1) {
		switch(opt ) {
		case 'd':
			debug=true;
			break;
		case 't':
			useFirst = false;
			break;
		case 'h':
			std::cerr << USAGE << std::endl;
			break;
		case 's':
			bufSize=atoi(optarg);
			break;
		}
	}

	if ( optind == argc ) {
		std::cerr << USAGE << std::endl;
	}
	int i=argc;

/* keep potential matches in the candidateList, remove when they no longer can be a match. */

	std::list<CandidateFile* > candidateList;
	CandidateFile *firstCandidate = NULL;
	CandidateFile *newFile;
	bool first=true;
	while( optind < argc ) {
		newFile=new CandidateFile(argv[optind]);
		if ( first )  {
			firstCandidate = newFile;
			if ( firstCandidate->candidate != true ) {
				std::cerr << "First file cannot be used " << std::endl;
			}
			first=false;
		} else {
			fileCnt = fileCnt + 1;
			if ( firstCandidate->size == newFile->size ) {
				candidateList.push_back(newFile);
			}
			else { 
				// the size does not match, so don't even try
				if ( debug ) {
					std::cerr << "Rejecting because size does not match " << newFile->fileName << std::endl;
					delete(newFile);
				}
			} 
			
		}
		optind++;
	}
	if ( firstCandidate == NULL || fileCnt == 0 ) {
		std::cerr << "Must specify at least two input files" << std::endl;
		exit(1);
	}
	// all matches were rejected due to size differences or files did not exist.
	if ( candidateList.size() == 0 ) {
		std::cerr << "No Matches Found" << std::endl;
		exit(0);
	}
	// read the file.  Maybe we should do more than one char at a time
	char c;
	char d;
	std::list<CandidateFile *>::iterator it;
	if ( firstCandidate->thisStream.is_open() ) {
		char *firstBuffer= (char *) malloc(bufSize+1);
		char *cmpBuffer=( char *) malloc(bufSize+1);
		while ( firstCandidate->thisStream ) {
			// read into first buffer
			firstCandidate->thisStream.read(firstBuffer,bufSize);
			int r1size= firstCandidate->thisStream.gcount();
			
			firstBuffer[r1size]='\000';
			for (it = candidateList.begin(); it != candidateList.end(); ++it){
				if ( (*it)->candidate ) {
					if ( (*it)->thisStream.is_open() ) {
						(*it)->thisStream.read(cmpBuffer,bufSize);
						int r2size= (*it)->thisStream.gcount();
						cmpBuffer[r2size]='\000';
						if ( strcmp(firstBuffer,cmpBuffer)  !=0 ) {
							if ( debug ) {
								std::cerr << "Rejecting contents are different " << (*it)->fileName << std::endl;
							}
							(*it)->candidate = false;
							// note we no longer need this element so we can erase it
							// -- to reset the iteratore after the erase
							(*it)->closeFile();
							candidateList.erase(it--);
						}
					}
				} else {
					if ( debug ) {
						std::cerr << "This should not happen" << (*it)->fileName << std::endl;
					}
				}
			}
		}
	}
	CandidateFile *oldestCandidate = firstCandidate;
	if ( ! useFirst  ) {
		int oldestTS = firstCandidate->mod_time;
		for (it = candidateList.begin(); it != candidateList.end(); ++it) {
			if ( (*it)->candidate ) {
				if ( (*it)->mod_time < oldestTS ) {
					oldestCandidate = (*it);
					oldestTS = (*it)->mod_time;
				}
			}
		}
		if ( firstCandidate != oldestCandidate ) {
			std::cout << firstCandidate->fileName << " " ;
		}
			
	}
	first=true;
	for (it = candidateList.begin(); it != candidateList.end(); ++it) {
		if ( (*it)->candidate && oldestCandidate != (*it) ) {
			if ( (*it) != oldestCandidate ) {
				if ( first and debug ) {
					std::cerr << "Got Match for base file"<< firstCandidate->fileName << std::endl;
				}
				// we probably want to remove dups, so it's easiest to have the followup script read line by line
				std::cout <<  (*it)->fileName << std::endl;
			}
		}
	}
		
}
