#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

int main(int argc, char** argv) {


	printf("|*** Convolution OpenMP program to get the edges of an image with parallelism ***|\n");

	/*
		Initialize variables. 
		The first ones are for the execution time, thread number and number of thread. 
		Also initialize counters that will be used.
	*/
	double wtime = 0.0;
	int id, p;
	int i = 0, j = 0, x = 0, y = 0;
	int position = 0;
	int pos_counter = 0;

	//Characteristics of the file. Will be used for both files so that they have the same specifics.
	unsigned char header[54];
	int width, height;
	
	/*
		Create instances for the arrays that will be used.
		pixel_zeros will get the image data as a 1D array to pass them to the I array.
		The A array which like the I is a 2D array, will be used to store the products of the convolution.

		The way the OpenMP works is, all the processes have a shared memory. That gives us the opportunity 
		to create a single array that the processes can modify without breaking it in portions. These instances
		will be initialized once by one process but all of the given processes will have access to them. In the end 
		when the convolution is done, only one thread will be used to save the values into the second file. We dont need to 
		gather any form of data, because of this shared memory trait. It is important to note that only a single process 
		will open and close files. This solves the problem of having the processes go write over one another and also the 
		programm would malfunction if this was the case. 
	*/
	int* pixel_zeros = NULL;
	int** I = NULL;
	int** A = NULL;
	/*
		Create the mask array necessary for convolution. 
		With the THREADS variable we get the given number for processes from the command line. 
		We then use it to set the number of threads using the omp_set_num_threads function. 
		Lastly the sum variable will be used as a temporary variable to hold the result from the 
		convolution equation and then pass it to the convolution array A. 
	*/
	int h[3][3] = { {0,1,0},{1,-4,1},{0,1,0} };
	int sum = 0;
	int THREADS = atoi(argv[1]);
	int value = 0, padding = 0;

	//Set the number of threads. 
	omp_set_num_threads(THREADS);

	/*
		The parallel part starts. From this point and until the portion of parallel is completed, 
		all the threads begin the execution. Just as mentioned before, the threads hold a shared memory 
		which we utilize here by passing the p variable = number of threads, because it will be useful later on. 
		Furthermore firstprivate clause is used to get the variables i, j, with the values already given by the
		first thread. This is not needed but is is good practice. Additionally, we are not using the private clause
		and the reason is that we want specific values that have been given once. 	
	*/
#pragma omp parallel shared(p) firstprivate(i,j) 
	{

		/*
			Get the number of given threads. We chech two things at this point. First, p isinclined to be an even number, 
			otherwise the program would fail and secondly, program is desinged to run for maximum 16 threads. Using more 
			would be counterproductive. Note that here return; command is not an option as the parallel #pragma rejects it as an option. 
			So we use exit statement instead;
		*/
		p = omp_get_num_threads();
		if ((p % 2 != 0)|| p > 16 ) {
			printf("Program cannot execute unless an even number of processes is given\n");
			exit(1);
		}

		/*
			Inside the parallel program the single utilization is useful when trying to have only one thread run this part. 
			In this instance, only one thread is allowed to open the file, retreive its data and then create the necessary 
			arrays. The rest of the threads will ignore this part. The thread running this portion is not certain that is the 
			master thread. Once the single portion ends the thread continues normaly its execution just like the others.		
		*/
#pragma omp single
		{
			/*
				Openning the file is the same as with the ohter two exercises. Use a FILE pointer to communicate with the file. 
				Open it with the option to read it (binary mode), check to see if the file was openned correclty and if that 
				is not what occured exit the program. 			
			*/
			FILE* fpin;
			fpin = fopen("image.bmp", "rb");
			if (fpin == NULL) {
				printf("Cannot Open File.Check if file is in the same directory as the program exe.\n");
				exit(1);
			}

			/*
				Read the header which contains all sorts of information about the file. 
				Get the sizes of the image and insert them to height and width variables. 
				Since the threads have a shared memory, these values are read by all processes. 
				Create a 1D array to get the average of every pixel from the image. 
				Create a small array to read the three values of every pixel.			
			*/
			fread(header, sizeof(unsigned char), 53, fpin);
			width = *(int*)&header[18];
			height = abs(*(int*)&header[22]);
			int* pixel = (int*)malloc(height * width * sizeof(int));
			unsigned char pixels[3];

			/*
				If pixel array is created normaly (check malloc allocation) then read from the 
				file with stride 1 and size three all the positions. Calculate the average of those values 
				and pass them into the array. 
			*/
			if (pixels) {
				for (i = 0;i < height;i++) {
					for (j = 0;j < width;j++) {
						fread(pixels, 3, 1, fpin);
						unsigned char gray = (pixels[0] + pixels[1] + pixels[2]) / 3;
						pixel[pos_counter] = (int)gray;
						pos_counter++;
					}
				}
			}else {
				printf("Malloc allocation failed. Terminating program...\n");
				exit(1);
			}

			/*
				Value variable to be mostly used when p = 8 or p = 16. Value is the count of rows 
				that will be added for the padding part later on. Of course it changes allongside p. 
				The padding variable holds the complete size of the added rows picture. Basically 
				fill a count of rows in the top and bottom of the pixel_zeros array so that the 
				array can be broken itno equal parts, every time with 100 columns. In addition, 
				the main part remains intact as the added rows are outside of its area of effect. 
				The program splits based on the p value because of the padding argument.
			*/
			value = (p - 4);
			padding = height * width + (value * height);
			//Split the program bade on the p value.
			if (p == 8 || p == 16) {

				/*
					Create the pixel_zeros 1D array. Check malloc allocation 
					If everything is normal, initialize this array with zeros in every position. 
				*/
				pixel_zeros = (int*)malloc(sizeof(int) * padding);
				if (pixel_zeros) {
					pos_counter = 0;
					position = 0;
					for (i = 0;i < value + height;i++) {
						for (j = 0;j < width;j++) {
							pixel_zeros[position] = 0;
							position++;
						}
					}

					/*
						Here pass the added rows and then fill the array with the contents of the image. 
						E.g for p = 8 the added rows are 4. Therefore in order to be symetrical insert two 
						of them into the bottom of the array and correspondingly the other two at the top. 
						They are filled with zeros, with the purpose to not affect the convolution. Lastly in 
						this part if position is inside these rows just pass. 					
					*/
					position = 0;
					for (i = 0; i < height + value;i++) {
						for (j = 0;j < width;j++) {
							if (position >= (height * (value / 2)) && position < ((height * (value / 2)) + (height * width))) {
								pixel_zeros[position] = pixel[pos_counter];
								pos_counter++;
							}
							position++;
						}
					}
				}else {
					printf("Malloc allocation failed. Terminating program...\n");
					exit(1);
				}

				/*
					Create the I array which will hold the values of the pixel zeros in a 2D format. 
					Check malloc allocation and because of the padding argument, malloc is using the value 
					variable to recreate the added rows. If everything is proceeding normally, initialize 
					it with zeros and istantly pass all the pixel_zeros data into it. Because I is a 2D 
					array malloc is split up to two parts and checked correspondingly. 
				*/
				position = 0;
				pos_counter = 0;
				I = (int**)malloc(sizeof(int*) * (height + value));
				if (I) {
					for (i = 0; i < height + value; i++) {
						I[i] = (int*)malloc(sizeof(int) * width);
						if (I[i] != NULL) {
							for (j = 0;j < width;j++) {
								I[i][j] = 0;
								I[i][j] = pixel_zeros[pos_counter];
								pos_counter++;
								position++;
							}
						}else {
							printf("Malloc allocation failed. Terminating program...\n");
							exit(1);
						}
					}
				}else {
					printf("Malloc allocation failed. Terminating program...\n");
					exit(1);
				}

				/*
					At this point thi I array is completed for p >= 8.
					Pixel_zeros is not needed anymore so clear the memory allocated for it. 
					Create the A 2D array with identical sizes as the I array. 
					Check malloc allocation and then initialize it with zeros.
					The A array is also ready for the convolution part. 
				*/
				free(pixel_zeros);
				A = (int**)malloc(sizeof(int*) * (height + value));
				if (A) {
					for (x = 0; x < height + value; x++) {
						A[x] = (int*)malloc(sizeof(int) * width);
						if (A[x] != NULL) {
							for (y = 0;y < width;y++) {
								A[x][y] = 0;
							}
						}else {
							printf("Malloc allocation failed. Terminating program...\n");
							exit(1);
						}
					}
				}else {
					printf("Malloc allocation failed. Terminating program...\n");
					exit(1);
				}
			}
			/*
				In the case where p = 4 or p = 2 the padding part is not needed anymore. 
				The array breaks into even parts, always with 100 columns. The procedure 
				is easier to pass the data from the image finally to the I array. 			
			*/
			else {

				/*
					First create the pixel_zeros to get the info from the image. As mentioned 
					in the code for the second exercise, this slice of code in not obligatory. 
					But for reasons of continuity and understanding it is kept and utilized. 
					Also noted that the thread after accessing the sizes for height and width 
					uses them to create the pixel_zeros with malloc. Check once again malloc
					allocation and then comes the I array making.
				*/
				pixel_zeros = (int*)malloc(sizeof(int) * height * width);
				pos_counter = 0;
				if (pixel_zeros) {
					for (i = 0;i < height;i++) {
						for (j = 0;j < width;j++) {
							pixel_zeros[pos_counter] = pixel[pos_counter];
							pos_counter++;
						}
					}
				}
				
				/*
					Creation of the I array in order to have the image data in a 2D 
					array format. Again because this is the practicality of the malloc 
					function we need two controls to se if everything is working as intended. 
					After the I is created is temporarilly initialized with zeros and immediately
					receives the values from the pixel_zeros array. After the for loop is completed
					the I array is ready. 
				*/
				I = (int**)malloc(sizeof(int*) * height);
				if (I) {
					for (i = 0; i < height; i++) {
						I[i] = (int*)malloc(sizeof(int) * width);
						if (I[i] != NULL) {
							for (j = 0;j < width;j++) {
								I[i][j] = 0;
								I[i][j] = pixel_zeros[position];
								position++;
							}
						}else {
							printf("Malloc allocation failed. Terminating program...\n");
							exit(1);
						}
					}
				}else {
					printf("Malloc allocation failed. Terminating program...\n");
					exit(1);
				}

				/*
					Create the A 2D array to get the products of the convolution part later on. 
					A array has the same size with the I array and is initialized with zeros 
					and then its ready for convolution. Always check malloc allocation for the 
					arrays. 				
				*/
				A = (int**)malloc(sizeof(int*) * height);
				if (A) {
					for (x = 0; x < height; x++) {
						A[x] = (int*)malloc(sizeof(int) * width);
						if (A[x] != NULL) {
							for (y = 0;y < width;y++) {
								A[x][y] = 0;
							}
						}else {
							printf("Malloc allocation failed. Terminating program...\n");
							exit(1);
						}
					}
				}else {
					printf("Malloc allocation failed. Terminating program...\n");
					exit(1);
				}
			}

			/*
				As the last step of the single parallel portion we need to free the arrays 
				that wont be used again. Also close the file connection and neutrilize the 
				counters. 
			
			*/
			free(pixel);
			fclose(fpin);
			position = 0;
			pos_counter = 0;
		}
	}

	/*
		The first parallel segment has finished and the products are the I array which withholds 
		the image data for all cases and the A array that will be the receiver of the convolution equation. 
		At this point the actual parallelization of the processes begin. First and foremost the parralel 
		decleration uses many clauses. Shared clause shows the elements that all the threads have available in 
		their united memory. This is crucial for the convolution to be parallel executed. Each thread will run
		a segment of the for loops and the elements that will eventually be stored into the A array will not 
		lack continuity, of course if the boundaries are organized.	The firstprivate clause is used so that 
		the variables needed for the loops and to get the time etc. are initialize with the values the first process 
		gives them. What this means is for example the wtime variable will only have 0.0 every time a thread 
		calls it because thats is value after the first thread creates it. 
	*/
#pragma omp parallel default(none) shared(I,A,h,height,width,padding) firstprivate(i,j,sum,id,p,x,y,wtime)
	{
		/*
			Get the execution start time for each thread. Begin here instead of the first parallel section 
			because even though they are called the p - 1 threads are not utilized there. So it is optimal to 
			get the time usage after the preprocessing of the arrays and image. With the id we get the thread 
			number to keep track. Additionally it helps with setting the boundaries associated to the for loop. 
			To explain it better, if for example we have 4 processes, the for loop is basically executed into 
			4 equal parts. The first process starts at 0 rows. The elements iterated for each process have to be 
			in this case 2500. Therefore the second process will have to start at row 25 and so on. This is 
			obligatory as the convolution is continues and the use of a shared memory is fully utilized. Also 
			The convoluted image does not break into parts but is constant. 		
		*/
		
		wtime = omp_get_wtime();
		id = omp_get_thread_num();
		
		//Get the counte of elements that every process will have to iterate. 
		int subarray = (height * width) / p;

		/*
			At this point and based on the p value the I array is either a simple 2D array with the sizes of 
			the image or has a few extra rows. For the second instance the subarray count will be assosiated
			with the padding of the I array. After subarray has been calculated for each picture, we get the
			number of rows for each process' iteration. Lastly we use the syb variable to set the start of 
			the iteration and the ending position. Note that only the size of rows changes as the columns 
			remain intact. 
		
		*/
		if (p == 8 || p == 16) {
			subarray = padding / p;
		}
		int sub = (subarray) / height;
		int start = (sub * id);//Start of iteration for each process
		int end = (sub * (id + 1));//End of iteration for each process.

		/*
			Just like the MPI program it is very important to have some "classification" for the convolution 
			part regarding the role of specific threads. So the convolution is divided to three stages, One for 
			the first part of the I array, one for the last part and the rest for the in between. The reasoning 
			behind this decision is that the set boundaries for the iteration will be altered for the corresponding 
			situation. 
		*/

		if (id == 0) {

			/*
				In the occasion where id is equal to zero you cannot start the convolution equation from the zero 
				position for the rows. The way the equation works it will try to find the element at x - 1 row. 
				It is obvious that if the iteration were to start at x = 0 it would search the I[-1] at a certain 
				point throwing a big segmentation fault in between. That is why the refrenced counter is set to 1 
				at the start. The rest of the procedure is almost the same as the other exercices. The sum variable 
				was added even though it was not imperative. It helped keep tracjing of the convolution. 
				The basic equation was not changed. 			
			*/
			for (x = 1;x < end; x++) {
				for (y = 1; y < width - 1; y++) {
					for (i = -1;i < 2;i++) {
						for (j = -1;j < 2;j++) {
							sum += h[j + 1][i + 1] * I[x - i][y - j];
							A[x][y] = sum;
						}
					}

					/*
						Neutrilize the sum variable. Check for any negative values in the convolution array A. 
						If there are any, turn them into zeros. 
					*/
					sum = 0;
					if (A[x][y] < 0) {
						A[x][y] = 0;
					}
				}
			}

			/*
				Convolution for the first part of the image is done and without any extra steps, it is stored 
				in the main A array for all threads to see. 
			*/
		}

		if (id == p - 1) {
			/*
				For the last process, the starting point is calculated before as shown and as 
				aforementioned, to keep the continuity of the pieces the iteration ends before reaching the 
				final row.The other reason is that the edges of the image are ignored and if this iteration 
				were to get to the last element, another segmentation fault would pop up. Because the final
				row for e.g 4 processes would be 100, and if the iteration were to hit that mark it would then 
				search the row I[101]. All the other aspects of this part are the same as the above convolution.			
			*/
			for (x = start;x < end - 1; x++) {
				for (y = 1; y < width - 1; y++) {
					for (i = -1;i < 2;i++) {
						for (j = -1;j < 2;j++) {
							sum += h[j + 1][i + 1] * I[x - i][y - j];
							A[x][y] = sum;
						}
					}
					sum = 0;
					if (A[x][y] < 0) {
						A[x][y] = 0;
					}
				}
			}
		}

		/*
			For the final step of the convolution, we handle the middle pack of processes. 
			The main point here is to not interfere with the other speciments of the procedure. 
			That is why each thread starts at its given start value and finishes just before it
			hits the end row which would then be the start of the next. One last time the all the 
			other aspects of the convolution part are the same as before.		
		*/
		if (id != 0 && id != p - 1) {
			for (x = start;x < end; x++) {
				for (y = 1; y < width - 1; y++) {
					for (i = -1;i < 2;i++) {
						for (j = -1;j < 2;j++) {
							sum += h[j + 1][i + 1] * I[x - i][y - j];
							A[x][y] = sum;
						}
					}
					sum = 0;
					if (A[x][y] < 0) {
						A[x][y] = 0;
					}
				}
			}
		}

		/*
			Get the final time of execution and the print it as requested for each thread. 
		*/
		wtime = omp_get_wtime() - wtime;
		printf("Time of execution for process %d ===> %f\n", omp_get_thread_num(), wtime);

	}

	/*
		The final parralel segment will be used to save the elements and products of the convolution 
		from the A array into the second file to then see the results. Only a single process will be used 
		for this part. Again because of the shared memory every process is capable for this role, not only 
		the master process. 
	*/
#pragma omp parallel shared(A, p,height,width) private(i,j,position)
	{
#pragma omp single
		{
			position = 0;
			value = (p - 4); // Get the number of added rows. 

			FILE* fpout; // Initialize the FILE pointer to establish the connection with the file.

			fpout = fopen("image_alter.bmp", "wb");//Open the file with the option to write in it. Binary mode.

			//If the file cannot or is not openned then exit the program. 
			if (fpout == NULL) {
				printf("Cannot Open File.Check if file is in the same directory as the program exe.\n");
				exit(1);
			}
			//Write the specifics of the first file into the second to match the expectations. 
			fwrite(header, sizeof(unsigned char), 54, fpout);

			/*
				For the final time the padding part is taken care of. While iterating the elements 
				of the A array pass the first and last height x (value/2) because they are zeros with no
				intended impact on the image. After that normally pass three times each value into the 
				file to represent the pixels.
			*/
			if (p == 16 || p == 8) {				
				for (i = 0;i < height + value; i++) {
					for (j = 0; j < width; j++) {
						if (position >= (height * (value / 2)) && position < ((height * (value / 2)) + (height * width))) {
							putc(A[i][j], fpout);
							putc(A[i][j], fpout);
							putc(A[i][j], fpout);
						}
						position++;
					}
				}

			}
			/*
				There is no padding here so normaly just pass the elements into the file, three times for every 
				position to represent the pixels. 
			*/
			else {
				for (i = 0; i < height; i++) {
					for (j = 0;j < width;j++) {
						putc(A[i][j], fpout);
						putc(A[i][j], fpout);
						putc(A[i][j], fpout);
					}
				}
			}

			/*
				Close the file stream and deallocate the columns of the I and A array.Note: 
				when trying to deallocate the full 2D array the program would crash, so it 
				was intentionally left out. 
			*/
			fclose(fpout);
			free(A);
			free(I);
		}
	}
	printf("|*** Program finished.To see the result open the file used to write the convoluted data. ***|\n");

	return 0;


}
