/*
  hello_world.c

  Copyright (C) 2012 Adapteva, Inc.
  Contributed by Yaniv Sapir <yaniv@adapteva.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program, see the file COPYING.  If not, see
  <http://www.gnu.org/licenses/>.
*/

// This is the HOST side of the Hello World example.
// The program initializes the Epiphany system,
// randomly draws an eCore and then loads and launches
// the device program on that eCore. It then reads the
// shared external memory buffer for the core's output
// message.

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <linux/fb.h>

#include <e-hal.h>

#define _BufSize   (128)
#define _BufOffset (0x01000000)
#define _SeqLen    (20)
#define FBDEV "/dev/fb0"
#define PAGE_OFFSET 0x2000
#define PAGE_SIZE 0x2000
#define FB "/dev/fb0"

typedef struct { 
	float x, y, z, vx, vy, vz, m, im;
} Body;

void draw_stars(Body* bufOutput, int nBodies, unsigned int *fp, int stride, int color);

static inline void nano_wait(uint32_t sec, uint32_t nsec)
{
  struct timespec ts;
  ts.tv_sec = sec;
  ts.tv_nsec = nsec;
  nanosleep(&ts, NULL);
}

void randomizeBodies(Body *data, int n) {
  int i;
  // Hard code first 2 stars as 'mega-stars'
  /*data[0].x = 384.0;
  data[0].y = 384.0;
  data[0].z = 384.0;
  data[0].vx = 0.0;
  data[0].vy = 0.0;
  data[0].vz = 0.0;
  data[0].m = 900000000.0;
  data[0].im = 1.0 / data[0].m;
  data[1].x = 640.0;
  data[1].y = 384.0;
  data[1].z = 384.0;
  data[1].vx = 0.0;
  data[1].vy = 0.0;
  data[1].vz = 0.0;
  data[1].m = 900000000.0;
  data[1].im = 1.0 / data[1].m;*/
  data[0].x = 512.0;
  data[0].y = 384.0;
  data[0].z = 200.0;
  data[0].vx = 0.0;
  data[0].vy = 0.0;
  data[0].vz = 0.0;
  data[0].m = 50000000.0;
  data[0].im = 1.0 / data[0].m;
  for (i = 1; i < n; i++) {
  /*  if(i == 1){
      data[i].x = 303.575623;
      data[i].y = 599.325623;
      data[i].z = 183.000000;
      data[i].vx = 2.000000;
      data[i].vy = -9.000000;
      data[i].vz = 18.000000;
      data[i].m = 100.000000;
      data[i].im = 1.0 / data[i].m;
    }
    else if(i == 2){
      data[i].x = 148.797012;
      data[i].y = 151.768005;
      data[i].z = 293.000000;
      data[i].vx = 11.000000;
      data[i].vy = -20.000000;
      data[i].vz = -2.000000;
      data[i].m = 100.000000;
      data[i].im = 1.0 / data[i].m;
    }
    else{*/
      data[i].x = 1024.0f * (rand() / (float)RAND_MAX) - 1.0f;
      data[i].y = 768.0f * (rand() / (float)RAND_MAX) - 1.0f;
      data[i].z = (float) (rand() % 768);
      data[i].vx = (rand() % 41) - 20.0f;
      data[i].vy = (rand() % 41) - 20.0f;
      data[i].vz = (rand() % 41) - 20.0f;
      data[i].m = 100.0f;
      data[i].im = 1.0 / data[i].m;
    //}
  }
}

static int fb;
static struct fb_fix_screeninfo fix;
static struct fb_var_screeninfo var;

int main(int argc, char *argv[])
{
	unsigned row, col, core;
	e_platform_t platform;
	e_epiphany_t dev;
  	int all_done, core_done;
	int i, x, y;
        int res;
        unsigned int *fp;


       fb = open(FB, O_RDWR);

       if (fb == -1) {
          perror("Unable to open fb " FB);
          return 1;
       }

       // rest here
       res = ioctl(fb, FBIOGET_FSCREENINFO, &fix);
       if (res != 0) {
          perror("getscreeninfo failed");
       	  close(fb);
          return 1;
       }

       //printf("framebuffer size %d @ %08x\n", fix.smem_len, fix.smem_start);

       res = ioctl(fb, FBIOGET_VSCREENINFO, &var);
       if (res != 0) {
          perror("getscreeninfo failed");
          close(fb);
          return 1;
       }

       //printf("size %dx%d @ %d bits per pixel\n", var.xres_virtual, var.yres_virtual, var.bits_per_pixel);

       fp = mmap(NULL, fix.smem_len, O_RDWR, MAP_SHARED, fb, 0);
       if (fp == (void *)-1) {
          perror("mmap failed");
          close(fb);
          return 1;
       }

       //printf("virtual @ %p\n", fp);

       int stride = var.xres_virtual;

	srand(time(NULL));
  	int nBodies = 30000;
	int startFlag = 0x00000001;
  	int iters = 5000;  // simulation iterations
  	const float dt = 0.05f; // time step
  	if (argc > 1) nBodies = atoi(argv[1]);
  	if (argc > 2) iters = atoi(argv[2]);


	Body *buf = (Body *) malloc(sizeof(Body) * nBodies);
	Body *bufOutput = (Body *) malloc(sizeof(Body) * nBodies);

  	randomizeBodies(buf, nBodies); // Init pos / vel data

	e_init(NULL);
	e_reset_system();
	e_get_platform_info(&platform);
	e_open(&dev, 0, 0, 4, 4);
	e_reset_group(&dev);
	e_load_group("e_rob_nbody.srec", &dev, 0, 0, 4, 4, E_FALSE);
    	for (row = 0; row < platform.rows; row++){
      		for (col = 0; col < platform.cols; col++){
			e_write(&dev, row, col, 0x00000004, &nBodies, sizeof(int));
      		}
    	}
	e_write(&dev, 0, 0, 0x1000, (Body *) buf, sizeof(Body) * nBodies);
	x = 0;
//	for(x = 0; x < iters; x++){
	while(1){
		//fprintf(stderr, "Iter %d\n", x);
    		for (row = 0; row < platform.rows; row++){
      			for (col = 0; col < platform.cols; col++){
				e_write(&dev, row, col, 0x00000008, &startFlag, sizeof(int));
      			}
    		}
		e_start_group(&dev);
  		//Check if all cores are done
  		while(1){
    			all_done = 0;
    			for (row = 0; row < platform.rows; row++){
      				for (col = 0; col < platform.cols; col++){
					e_read(&dev, row, col, 0x00000008, &core_done, sizeof(int));
					all_done += core_done;
      				}
    			}
    			if(all_done == 0){
      				break;
    			}
  		}
		e_read(&dev, 0, 0, 0x1000, (Body *) bufOutput, sizeof(Body) * nBodies);
		if(x != 0){
			draw_stars(buf, nBodies, fp, stride, 0x00000000);
		}
		else{
			x = 1;
		}
		draw_stars(bufOutput, nBodies, fp, stride, 0x00ffffff);
		memcpy(buf, bufOutput, sizeof(Body) * nBodies);
	}
	e_close(&dev);

	// Release the allocated buffer and finalize the
	// e-platform connection.
	e_finalize();

	return 0;
}

void draw_stars(Body* bufOutput, int nBodies, unsigned int *fp, int stride, int color)
{
	int y, s_x, s_y, s_z;
	for (y = 0; y < nBodies; y++){
		s_x = (int) (bufOutput[y].x);
		s_y = (int) (bufOutput[y].y);
		s_z = (int) (bufOutput[y].z);
		if(s_x >= 0 && s_x < 1024 && s_y >= 0 && s_y < 768){
			if(y < 1){
       				fp[s_x + s_y * stride] = (color == 0x00000000 ? color : 0x00ff0000);
				if(s_x > 0)
       					fp[(s_x - 1) + s_y * stride] = (color == 0x00000000 ? color : 0x00ff0000);
				if(s_x < 1022)
       					fp[(s_x + 1) + s_y * stride] = (color == 0x00000000 ? color : 0x00ff0000);
				if(s_x > 1)
       					fp[(s_x - 2) + s_y * stride] = (color == 0x00000000 ? color : 0x00ff0000);
				if(s_x < 1021)
       				fp[(s_x + 2) + s_y * stride] = (color == 0x00000000 ? color : 0x00ff0000);
				if(s_y > 0)
       					fp[s_x + (s_y - 1) * stride] = (color == 0x00000000 ? color : 0x00ff0000);
				if(s_y < 766)
       					fp[s_x + (s_y + 1) * stride] = (color == 0x00000000 ? color : 0x00ff0000);
				if(s_y > 1)
       					fp[s_x + (s_y - 2) * stride] = (color == 0x00000000 ? color : 0x00ff0000);
				if(s_y < 765)
       					fp[s_x + (s_y + 2) * stride] = (color == 0x00000000 ? color : 0x00ff0000);
			}
			else
				if(color == 0x00000000){
             				fp[s_x + s_y * stride] = color;
				}
				else{
					/*if(bufOutput[y].vz < -50.0){
             					fp[s_x + s_y * stride] = 0x00ff0000;
					}
					else if(bufOutput[y].vz > 50.0){
             					fp[s_x + s_y * stride] = 0x000000ff;
					}
					else if(bufOutput[y].vz >= -50.0 && bufOutput[y].vz < 0.0){
             					fp[s_x + s_y * stride] = 0x00ffb6c1;
					}
					else if(bufOutput[y].vz > 0.0 && bufOutput[y].vz <= 50.0){
             					fp[s_x + s_y * stride] = 0x0087cefa;
					}
					else{
             					fp[s_x + s_y * stride] = 0x00ffffff;
					}*/
             				fp[s_x + s_y * stride] = 0x00ffffff;
				}
		}
	}
}
