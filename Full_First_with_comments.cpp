#define _CRT_SECURE_NO_WARNINGS 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>  

int main(int argc, char** argv) {

	printf("|*** Convolution program to get the edges of an image ***|\n");

	//Initialization of the time variable to measure the execution of the convolution part. 
	double time_execution = 0.0;
	//The clock() function is utilized. The clock() function returns the approximate processor time that is consumed by the program. 
	clock_t begin = clock();

	/*
	  Initialization of file pointers to access the image.
	  Need two pointers in order to open each bmp image. 
	  The image has been processed to be 100 x 100 and to be a bmp file for easier access to the pixels.
	*/
	FILE* fpin;
	FILE* fpout;

	/*
	  Open the two files. 
	  For the first we need to just read the data of the image. 
	  For the second if the file does not exist, then create it to write in it.  
	  "rb" and "wb" are used to read and write in binary mode. 
	 */
	fpin = fopen("image.bmp", "rb");
	fpout = fopen("image_alter.bmp", "wb");

	//Check to se if the files openned correctly. If not then terminate the program.
	if (fpin == NULL || fpout == NULL) {
		printf("The files did not open corectly.\nCheck to see if the names are correct\nand if the files are in the file of the executable.\n");
		return 0;
	}

	/*
		Generic code to read the characteristics of the file.
		We need this to read the size of the file and get the values to pass them on the second file.
		Also it is used so that the second file is of the same type as the first. 
	*/
	unsigned char header[54];
	fread(header, sizeof(unsigned char), 53, fpin);
	fwrite(header, sizeof(unsigned char), 54, fpout);

	/*
		Get the height and width value for the read header. The positions of the sizes are the same each time. 
		If the header is printed to check the values, the sizes are there for every file needed to open. 
	*/
	int width = *(int*)&header[18];
	int height = abs(*(int*)&header[22]);

	/*
		Here begins the process to retreive the data and insert them into a 1D array.
		From the sizes taken the array is created using malloc to dynamically allocate the memory. 
		The array will have for a 100x100 sizes the capacity of 10.000 integers. 
		Then a second array is created to get the three values that the bmp image contains for each pixel. 
		That's because it is an rgb image. Even though the values from the image are integers it is safer to read 
		them like an unsigned character. So other files are edible too. 
		After that, a check to see if the malloc allocation was succesfull is required. If not then the program will 
		terminate.
	*/
	int* pixel = (int*)malloc(height * width * sizeof(int));
	unsigned char pixels[3];
	int i, j;
	int pos_counter = 0;
	if (pixel) {
		/*
			Run the iteration in the format of a 2D array. This is not necessary but it helps to have continuity later
			when we pass the elements to the 2D array. The result is that, we take 3 values from the image for each pixel 
			and the easiest way to get a logical result is to get the mean of the three elements. And for that to happen 
			the file is readed with stride 1 and element size 3. After we get the average we insert it into the 1D array.
			The pos_counter value is used to iterate the 1D file correctly. 
		*/
		for (i = 0;i < height;++i) {
			for (j = 0;j < width;++j) {
				fread(pixels, 3, 1, fpin);
				int gray = int((pixels[0] + pixels[1] + pixels[2]) / 3);
				pixel[pos_counter] = gray;				
				pos_counter++;
			}
		}	
	}else {
		printf("Malloc allocation failed. Terminating program...\n");		
		return 0;
	}
	
	pos_counter = 0;

	/*
		Creation of the I 2D array. It has the size of the image. 
		The malloc allocation for a 2D array is as follows. Firstly create
		the "rows" of the array that will store pointers (array with 100 elements)
		If the malloc allocation occurs then proceed with creating the second pointers. 
		As the malloc is making the rows of I, we check to see of that row is founded 
		and it is not null. If all goes as planned then pass the values of the 1D array 
		to the I, which willl be used later for the convolution.
	
	*/
	int** I = (int**)malloc(height * sizeof(int*));
	if (I) {
		for (i = 0;i < height;i++) {
			I[i] = (int*)malloc(width * sizeof(int));
			if (I[i] != NULL) {
				for (j = 0;j < width;j++) {
											
					I[i][j] = pixel[pos_counter];					
					pos_counter++;
				}
			}else {
				printf("Malloc allocation failed. Terminating program...\n");
				return 0;
			}
		}
	}else {
		printf("Malloc allocation failed. Terminating program...\n");
		return 0;
	}


	pos_counter = 0;

	/*
		Deallocate the memory for the 1D array. 
		Close the conection with the file.	
	*/
	free(pixel);
	fclose(fpin);

	/*
		Creation of the mask.Normally it is needed to process this maskand get the transposeand reversed version of it.
		But in this occasion the mask would not change. Also in the convolution equation that was given the H array was used.
	*/
	int h[3][3] = { {0,1,0},{1,-4,1},{0,1,0} };
	int x, y;

	/*
		Creation of the 2D array to store the data from the convolution of the I and h arrays. 
		It has the same size as the I array, even though it this is not required as the convolution 
		will not produce data for all the elements of the I array. The procedure to make the array is the 
		same sa before. Also it is necessary to initialize the array and fill it with zeros as it is a neutral value. 
	*/
	int** A = (int**)malloc(height * sizeof(int*));

	//Check malloc allocation 
	if (A) {
		for (x = 0;x < height;x++) {
			A[x] = (int*)malloc(width * sizeof(int));
			if (A[x] != NULL) {
				for (y = 0;y < width;y++) {
					A[x][y] = 0;
				}
			}else {
				printf("Malloc allocation failed. Terminating program...\n");
				return 0;
			}
		}
	}else {
		printf("Malloc allocation failed. Terminating program...\n");
		return 0;
	}

	/*
		Here the convolution part begins. 
		It starts in the second row and second column of the I array and ends at the second to last 
		row/column. The reasoning, is that the process of convolution uses the elements of the array thar 
		surround the value that is being convoluted. That means that the iteration will fail in other circumstances, 
		as we get out of the boundaries for I. Henceforth a segmentation fault will pop up. 
		The above situation actually helps the image processing, because the value of the edges are turned into zeros 
		and basically are ignored. 
	
	*/
	for (x = 1;x < height - 1;x++) {
		for (y = 1;y < width - 1;y++) {
			//This part is given in the excersice report.
			for (i = -1;i < 2;i++) {
				for (j = -1;j < 2;j++) {
					//The equation was also given and for this question was not changed. 
					A[y][x] += h[j + 1][i + 1] * I[y - j][x - i];
				}
			}
		}
	}
		

	/*
		The I array is not demanded anymore so deallocate the 2D array. 
		First each column and then each row. 	
	*/
	for (i = 0;i < height;i++) {
		free(I[i]);
	}free(I);


	//Check to see if the convolution produced any negative results and turn them to zero.
	for (x = 0;x < height;x++) {
		for (y = 0;y < width;y++) {
			if (A[x][y] < 0) {
				A[x][y] = 0;
			}
		}
	}
	/*
		To return the values to the second file it is very important to 
		retain the format of the three values for every pixel / position of 
		the file. So we use putc function to insert a single element to the position. 
		Run the iteration for the whole A array, where the edges are zero.
		Therefore three consecutive putc calls are used. 
	*/
	for (i = 0;i < height;i++) {
		for (j = 0;j < width;j++) {
			putc(A[i][j], fpout);
			putc(A[i][j], fpout);
			putc(A[i][j], fpout);
		}
	}

	//Get finish time
	clock_t end = clock();
	
	//Calculate the execution time.To get the number of seconds used by the CPU, we will need to divide by CLOCKS_PER_SEC.
	//CLOCKS_PER_SEC is defined to be one million in <time. h>
	time_execution += (double)(end - begin) / CLOCKS_PER_SEC;
	printf("\t|Time of Execution is %f|\n", time_execution);

	//Deallocate the A 2D array just like the I array. 
	for (i = 0;i < height;i++) {
		free(A[i]);
	}free(A);

	//Close the connection with the second file. 
	fclose(fpout);

	printf("|*** Program finished.To see the result open the file used to write the convoluted data. ***|\n");
	return 0;

}


