#include "stdafx.h"
#include <string>
#include <iostream>
#include <vector>
#include <sstream>
#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <memory.h>
#include <time.h>
#include <sys/timeb.h>
#include <m3api/xiapi_dng_store.h> // Linux, OSX


#define HandleResult(res,place) if (res!=XI_OK) {printf("Error after %s (%d)\n",place,res);exit(1);}
#define PORT 50009

float timedelta(const timeb& finish, const timeb& start){
	float t_diff = (float) (1.0 * (finish.time - start.time)
        	+ 0.001*(finish.millitm - start.millitm));
	return t_diff;
}

int MsgDecode(const std::string& data, 
			  std::string& name, 
			  int& length, 
			  int& fps){
    std::stringstream container(data);
    container >> name;
    if (name == "close"){
        	//shutdown(server_fd, SHUT_RDWR);
        	//close(new_socket);
        	//close(server_fd);
        	exit(0);
    }
    container >> length;
    container >> fps;
    return 0;
}


XI_RETURN InitializeCameras(HANDLE& cam1, const int& fps) {
	XI_RETURN stat = XI_OK;

	xiSetParamInt(0, XI_PRM_AUTO_BANDWIDTH_CALCULATION, XI_OFF);
	// open the cameras
	

	stat = xiSetParamInt(cam1, XI_PRM_IMAGE_DATA_FORMAT, XI_RAW8);
	HandleResult(stat,"xiSetParam (image format)");
	
	
	xiSetParamInt(cam1, XI_PRM_ACQ_TIMING_MODE, XI_ACQ_TIMING_MODE_FRAME_RATE);
	xiSetParamFloat(cam1, XI_PRM_FRAMERATE, fps);
	

	stat = xiSetParamInt(cam1, XI_PRM_EXPOSURE, 14000);
	HandleResult(stat,"xiSetParam (exposure set)");

	return XI_OK;
}

void SaveFiles(const std::string& name){
	std::string create_dir1 = "mkdir " + name;
	const char *dir1 = create_dir1.c_str();
	system(dir1);

	std::string move1 = "mv cam1*.dng "+name+"/";
	const char *mv1 = move1.c_str();
	system(mv1);
	return;
}

XI_RETURN MakeRecording(const std::string& name, 
						const int& rec_length, 
						const int& fps, 
						const HANDLE& cam1) {
	XI_IMG img1;

	memset(&img1,0,sizeof(img1));
	img1.size = sizeof(XI_IMG);
	
	XI_RETURN stat;
	time_t begin, end;
	float t_diff = 0;
	struct timeb t_start, t_current;
	double seconds = 0.;
	
	int i = 0;
	time(&begin);
	ftime(&t_start);
	while(seconds < (float)rec_length){
		printf("%f\n", seconds);
		std::string fname1 = "cam1" + name + std::to_string(10000+i) + ".dng";
		
		const char *c1 = fname1.c_str();
		
		ftime(&t_current);
		
		if (timedelta(t_current, t_start) >= (float)(1.0/fps)){
			t_start = t_current;
			
			stat = xiGetImage(cam1, 5000, &img1);
			HandleResult(stat,"xiGetImage (img)");
			
			XI_DNG_METADATA metadata1;
			xidngFillMetadataFromCameraParams(cam1, &metadata1);
			
			stat = xidngStore(c1, &img1, &metadata1);
			
			time(&end);
	      	seconds = difftime(end, begin);
	      	++i;
      	}
	}
	return stat;
}

void LaunchRec(const std::string& name, 
			   const int& rec_length, 
			   const int& fps){
	HANDLE cam1 = NULL;
	
	XI_RETURN stat = XI_OK;
	
	// Retrieving a handle to the camera device
	printf("Opening cameras...\n");
	
	xiOpenDevice(0, &cam1);
	

	stat = InitializeCameras(cam1, fps);
	HandleResult(stat,"InitializeCameras");
	
	std::cout << "Starting acquisition..." << std::endl;
	
	stat = xiStartAcquisition(cam1);
	HandleResult(stat,"xiStartAcquisition");
	
	// Make a recording with n_frames
	stat = MakeRecording(name, rec_length, fps, cam1);
	
	std::cout<<"Stopping acquisition"<<std::endl;
	
	xiStopAcquisition(cam1);
	xiCloseDevice(cam1);

	SaveFiles(name);
	std::cout << "Done" << std::endl;
}

void Server(){
	int server_fd, new_socket, valread; 
    struct sockaddr_in address; 
    int opt = 1; 
    int addrlen = sizeof(address); 
    char buffer[1024] = {0}; 
    const char *hello = "RECORDING BEING LAUNCHED\n";

    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){ 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons( PORT ); 

    if (bind(server_fd, (struct sockaddr *)&address,  
                                 sizeof(address))<0) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    }
    printf("Welcome to remote NUC recorder at port  %d\n", PORT);

    while(1) {
        if (listen(server_fd, 3) < 0){ 
            perror("listen"); 
            exit(EXIT_FAILURE); 
        }

        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
                           (socklen_t*)&addrlen))<0) { 
            perror("accept"); 
            exit(EXIT_FAILURE); 
        }

        printf("ACCEPTING...\n");
        send(new_socket , "Ready\n" , 7 , 0 );
        
        valread = read(new_socket , buffer, 1024); 
        int length = 0, fps = 0;
        std::string name;
        
        MsgDecode(buffer, name, length, fps);
        
        LaunchRec(name, length, fps);
        send(new_socket , "Ready\n" , 7 , 0 );
    	//shutdown(server_fd, SHUT_RDWR);
        close(new_socket);
    }
}

int main(int argc, char* argv[]){
	Server();
	return 0;
}

