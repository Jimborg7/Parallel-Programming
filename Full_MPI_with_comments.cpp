#define _CRT_SECURE_NO_WARNINGS 
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char** argv) {


	//Initialize the basic varriables to be used from all threads.
	double wtime = 0.0;
	int i, j, x, y;
	int id, p, ierr;
	unsigned char header[54] = { NULL };

	//Initialize width and height with the sizes of the image and the necessary counters.
	int width = 100, height = 100;
	int pos_counter = 0;
	int position = 0;

	//Initialize the main arrays to be used in the program.
	int* pixel_zeros = NULL;
	int** I = NULL;
	int** A = NULL;
	const int master = 0;

	//Array to be used for the MPI_Gather function to collect the part from each process.
	int* gather = NULL;

	/*
		Check to see if the INIT function is normally executed.
		If MPI INIT returns 1 then the initialization has failed.
		For that to occur, maybe something went wrong with the processes
		or the accountants given from the command line.
	*/
	ierr = MPI_Init(&argc, &argv);
	if (ierr) {
		printf("Mpi Initialization failed. Exit program\n");
		MPI_Finalize();
		exit(1);
	}

	//Get the id for each process. 
	MPI_Comm_rank(MPI_COMM_WORLD, &id);
	//Get the total number of processes that will be used. 
	MPI_Comm_size(MPI_COMM_WORLD, &p);


	/*
		Because a 100 x 100 image cannot be divided by 8 or 16 and produce a X x 100 array
		the technique of padding is implemented. Basically, we add a number of rows to the array
		that will store, for the convolution, the elements from the image. The critical point is to share the
		added rows evenly top to bottom. So for example if we add 4 rows, two of them will be at the top
		and the rest will be at the bottom. For padding to be made for 8 and 16 processes, p - 4 is the value
		of the added rows that will be added. E.g. for 8 it's 4 and for 16 it's 12.
	*/
	int padding = height * width + ((p - 4) * height);

	/*
		Check the number of processes given by the user. If the argument for processe is more than 16
		or it is an odd number then the program will exit immediately.
	*/
	if ((p % 2 != 0) || p > 16) {
		printf("Program cannot execute unless an even number of processes is given\n");
		MPI_Finalize();
		return 0;
	}

	//Print a welcoming message. This will not necessarily be the first line in the output.
	if (id == 0) {
		printf("|*** Convolution MPI program to get the edges of an image with parallelism ***|\n");
	}

	//Get start time of execution for each process.
	wtime = MPI_Wtime();

	//Pointers for opening the image file and saving it in another file.
	FILE* fpin;
	FILE* fpout;

	//Synchroniize the processes to mostly wait whiile the first process gets & creates the compartments. 
	MPI_Barrier(MPI_COMM_WORLD);

	//Work for the first process with id -> 0
	if (id == 0) {

		//Open the File and read from it. Again use of "rb" to read in binary mode. Check to see if it was openned correctly

		fpin = fopen("image.bmp", "rb");
		if (fpin == NULL) {
			printf("Cannot Open File.Check if file is in the same directory as the program exe.\n");
			MPI_Finalize();
			return 0;
		}

		//Read the elements that contain the characteristics of the file in order to get the sizes.
		fread(header, sizeof(unsigned char), 53, fpin);

		/*
			Even though the heightand width are initialiazed in the future this will help the program
			to get the correct sizes in other images. The key here is for the 0 process to return and send
			these parts to the rest of the processes with send.
		*/
		width = *(int*)&header[18];
		height = abs(*(int*)&header[22]);

		//Here we create the 1D array to insert the elements from the file.
		int* pixel = (int*)malloc(sizeof(int) * height * width);

		/*
			Just like the previous excercise we initialize an array
			to read the 3 values for each pixel. If the malloc allocation was succesful
			we read each elements with stride 1 and size 3. We then calculate
			the average value of the three rgb values for every pixel.After,
			store the value from the image into 1D array.
		*/
		unsigned char pixels[3];
		if (pixel) {
			for (i = 0; i < height;i++) {
				for (j = 0; j < width; j++) {
					fread(pixels, 3, 1, fpin);
					unsigned char gray = (pixels[0] + pixels[1] + pixels[2]) / 3;
					pixel[pos_counter] = (int)gray;
					pos_counter++;
				}
			}
		}
		else {
			printf("Malloc allocation failed. Terminating program...\n");
			MPI_Finalize();
			return 0;
		}

		/*
			The padding part is executed here. If the given number of processe is 8 or 16
			accordingly, where we create a bigger than usually 1D array to insert the information
			from the image. However store it in a way, that these elements are stored after the added rows.
			In the case, where for some reason the added rows were deleted the remaining array would
			be edible to recreate the full picture for less processes though. At last we initialize the pixel_zeros
			array with zeros, and then we run the iteration, again in the format of a 2D array. As mentioned before
			for the first and last couple of rows, leave them as they are (full of zeros), to then normally pass
			the values to the pixel_zeros.
			E.g. For 8 processes the rows are 4 and so two rows at the top and two at the bottom. In other words
			ignore 200 elements in the beginning and another 200 elements before finishing.
		*/
		if (p == 8 || p == 16) {
			pixel_zeros = (int*)malloc(sizeof(int) * padding);
			if (pixel_zeros) {
				pos_counter = 0;
				position = 0;
				for (i = 0;i < height + (p-4);i++) {
					for (j = 0;j < width;j++) {
						pixel_zeros[position] = 0;
						position++;
					}
				}
				position = 0;
				for (i = 0;i < height + (p - 4);i++) {
					for (j = 0;j < width;j++) {
						position++;
						//Ignore the elements ouside the if statement.
						if (position >= (height * ((p - 4) / 2)) && position < ((height * ((p - 4) / 2)) + (height * width))) {
							pixel_zeros[position] = pixel[pos_counter];
							pos_counter++;
						}
					}
				}
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			/*
				In the case where p is less than 8 then padding is not needed. Therefore just like in the
				first question. Create the 1D array to store the information. Maybe this step is not needed
				because we just pass the values from a 1D array to another 1D but it is safer for continuity
				purposes.
			*/
		}
		else {
			pixel_zeros = (int*)malloc(sizeof(int) * height * width);
			pos_counter = 0;
			if (pixel_zeros) {
				pos_counter = 0;
				for (i = 0; i < height;i++) {
					for (j = 0;j < width;j++) {
						pixel_zeros[pos_counter] = pixel[pos_counter];
						pos_counter++;
					}
				}

			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}
		}

		/*
			After the interchanging of the values is done then free the allocated space
			for the pixel array, bring the counters to zero and close the connection to the read file.
		*/
		pos_counter = 0;
		position = 0;
		free(pixel);
		fclose(fpin);
		printf("| Succesfully preprocessed the image elements. |\n");
	}

	//After the 0 process is completed, synchronize the processes
	MPI_Barrier(MPI_COMM_WORLD);

	/*
		First and foremost, recreate the mask to be multiplied fro the convolution part.
		Make two variables to be used in order to set the boundaries for the scatter. What this means
		is, that with the MPI_Scatter Function the complete array will be separated to equal subarrays.
		It is therefore crucial to set the amount of values that each subarray will contain. This is the
		size to be sent variable. The sub_height variable is the count of rows that each subarray will be
		later created with. Of course, for the 8, 16 cases the added rows are included to not comprimise
		the iterations and the 2D arrays.
	*/
	int h[3][3] = { {0,1,0},{1,-4,1},{0,1,0} };
	int size_to_be_sent = 0;
	int sub_height = 0;

	if (p == 8 || p == 16) {
		size_to_be_sent = padding / p;
		sub_height = (int)(size_to_be_sent / height);
	}
	else {
		size_to_be_sent = (height * width) / p;
		sub_height = (int)(size_to_be_sent / height);
	}

	//Creation of the subarray to collect the corresponding part from the main pixel_zeros array.
	int* subarray = (int*)malloc(sizeof(int) * size_to_be_sent);
	if (!subarray) {
		printf("Malloc allocation failed. Terminating program...\n");
		MPI_Finalize();
		return 0;
	}

	/*
		MPI_Scatter() is used to, as the name suggests, scatter the main array to corresponding sub arrays
		in order every thread to get a same portion. For example, for p = 4 every subarray counts 2500 elements
		As a result, they will have 25 rows each. This applies in all the cases with the slight complication
		of the padding execution.
	*/
	MPI_Scatter(pixel_zeros, size_to_be_sent, MPI_INT, subarray, size_to_be_sent, MPI_INT, master, MPI_COMM_WORLD);

	/*
		In this part of the code two illustration are provided.The reasoning is that in the later stages of the program
		many sendand receives are bound to be executed.For p = 2 processes, we send from the first to the second
		process the part of this will be discused afterwards. For p>2 the in-between processes have to execute more
		send & receive requests. It is important to note that all these sent and received elements that one process sents
		to the other, are ignored during the convolution part.
	*/
	if (p == 2) {

		//MPI_Status is necessary for the Recv function.
		MPI_Status status;

		/*
			Divide the program based on the id also known as the subarrays.
			For the instance of id = 0, create the aforementioned 2D array.
			This will store the values of the subarray but it is important to notice that
			it is made with one additional row. The thought process is this:
			Id(0) will sent the last 100 elements to the next id. The convolution part requires this and
			explanation is continuity. Suppose we did not send them, convolution would produce different results
			in the separation line. And even more difficult to handle would be the fact that which row is the
			correct one to keep and eventually save to the file. Only with scatter the image cannot be convoluted.
		*/
		if (id == 0) {
			//Create array with additional row. Check for malloc allocation both times.
			I = (int**)malloc(sizeof(int*) * (sub_height + 1));
			if (I) {
				for (i = 0; i <= sub_height; i++) {
					I[i] = (int*)malloc(sizeof(int) * height);
					if (I[i] != NULL) {
						for (j = 0; j < height; j++) {
							I[i][j] = 0;
							position++;
						}
					}
					else {
						printf("Malloc allocation failed. Terminating program...\n");
						MPI_Finalize();
						return 0;
					}
				}
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			/*
				After the initialization of the I array and filling it with zeros, we pass the scatter elements
				into it. The count, again, varies for the different p. The logic here is to fill all but the last rows.
				e.g for p = 2 subarray has 5000 elements. The I can hold up to 5100 elements. Therefore the 5000 elements are stored
				leaving the last row empty and ready to get the 100 elements from the next process.
			*/
			for (i = 0; i < sub_height;i++) {
				for (j = 0; j < height; j++) {
					I[i][j] = subarray[pos_counter];
					pos_counter++;

				}
			}

			/*
				This part is used to retreive the last 100 elements that came with the subarray, create a 1D
				array to put them into and then send this to the next process. The position initialized to
				start from the 5000 - 100 = 4900 position and give the values to the lastelems array.
			*/
			position = size_to_be_sent - height;
			pos_counter = 0;
			int* lastelems = (int*)malloc(sizeof(int) * height);
			if (lastelems) {
				for (j = 0; j < height; j++) {
					lastelems[j] = subarray[position];
					position++;
				}
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			position = 0;

			/*
				The lastelems now holds the 100 last values of the subarray. The height variable is almost always
				used for future implemantations. Here the height is 100. With the MPI_Send function we send the
				compartments of the lastelems array to the next process.
			*/
			MPI_Send(lastelems, height, MPI_INT, id + 1, 1, MPI_COMM_WORLD);

			//Not necessary but it is good practice to neutrilize the lastelems array that will receive the 100 elements. 
			for (j = 0; j < height; j++) {
				lastelems[j] = 0;
			}

			/*
				The next process will send back its first 100 subarray elements, which in turn
				will be the last I elements of this process. That is why after receiving them
				immediately pass the to the I array.
			*/
			MPI_Recv(lastelems, height, MPI_INT, id + 1, 1, MPI_COMM_WORLD, &status);
			for (j = 0;j < height;j++) {
				I[sub_height][j] = lastelems[j];
			}

			//Deallocate the memory for the lastelems 1D array
			free(lastelems);
			printf("|Finished preparing and sending data for process %d|\n", id);
			/*
				After all that the I array is ready to be convoluted providing continuity
				and the correct values for the A. It is important to note that in the MPI
				every process has each own memory part and that is highly noticable when
				this process goes straight for the convolution having each unique part of
				the subarray and at last its unique convolution array.
			*/
		}
		else {

			/*
				This part is the same as for the other process.
			*/
			I = (int**)malloc(sizeof(int*) * (sub_height + 1));
			if (I) {
				for (i = 0; i <= sub_height; i++) {
					I[i] = (int*)malloc(sizeof(int) * height);
					if (I[i] != NULL) {
						for (j = 0; j < height; j++) {
							I[i][j] = 0;
						}
					}
					else {
						printf("Malloc allocation failed. Terminating program...\n");
						MPI_Finalize();
						return 0;
					}
				}
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			/*
				This part is different. The first step here is to receive the sent elements from the
				previous process. Create a 1D array to store 100 elements and straight away get the
				Recv function result. These elements will play the part of the first 100 elements of this
				processes I instance. To conclude the recv_elements has size of 100 integers, receives
				100 elements from the MPI_Recv function and passes them to the I array.
			*/
			int* recv_elements = (int*)malloc(sizeof(int) * height);
			if (recv_elements) {
				MPI_Recv(recv_elements, height, MPI_INT, id - 1, 1, MPI_COMM_WORLD, &status);
				for (j = 0; j < height; j++) {
					I[0][j] = recv_elements[j];
				}
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			/*
				At this point we have 100 elements for the first row of the I array.For it to be ready
				we get the subarray values all into the I. Careful not to overwrite the first row, the iteration
				starts at row = 1.At the end of the for loop the I in this case has 5100 elements.
			*/
			for (i = 1; i <= sub_height; i++) {
				for (j = 0; j < height; j++) {
					I[i][j] = subarray[position];
					position++;
				}
			}
			position = 0;

			/*
				While all this preprocessing is occuring, the previous process (id = 0) is waiting to receive
				the first 100 elements of this process's subarray. Therefore we transfer the elements from the
				subarray to the recv_elements and to clarify these are the last 100 of the subarray. To finish,
				we send the recv_elements array back to the other process.
			*/
			for (j = 0; j < height; j++) {
				recv_elements[j] = subarray[position];
				position++;
			}
			MPI_Send(recv_elements, height, MPI_INT, id - 1, 1, MPI_COMM_WORLD);
			printf("|Finished preparing and sending data for process %d|\n", id);

		}
	}
	/*
		As aforementioned for p>2 the in-between processes have to execute more
		send & receive requests. This does not apply to the first and last processes, because
		they communicate exclusively with one other process. Therefore the part for these processes
		are the same with the case of p>2.
	*/

	else if (p > 2) {

		//Use the tag variable to hold the communication "id" for every send & recv. 
		pos_counter = 0;
		position = 0;
		MPI_Status status;
		int tag = 1;

		if (id == 0) {

			/*
				Create the I array with the aditional array.
				Check malloc allocations.
				Fill it with zeros.
			*/
			I = (int**)malloc(sizeof(int*) * (sub_height + 1));
			if (I) {
				for (i = 0;i <= sub_height;i++) {
					I[i] = (int*)malloc(sizeof(int) * height);
					if (I[i] != NULL) {
						I[i][j] = 0;
					}
					else {
						printf("Malloc allocation failed. Terminating program...\n");
						MPI_Finalize();
						return 0;
					}
				}
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			/*
				Get the subarray elements into the I but leave the last row.
				Position set to start at the first elements of the last 100
				from the subarray. Create the lastelems array ans pass it
				those values.
			*/ 
			for (i = 0; i < sub_height;i++) {
				for (j = 0;j < height;j++) {
					I[i][j] = subarray[pos_counter];
					pos_counter++;
				}
			}
			position = size_to_be_sent - height;
			pos_counter = 0;
			int* lastelems = (int*)malloc(sizeof(int) * height);
			if (lastelems) {
				for (j = 0;j < height;j++) {
					lastelems[j] = subarray[position];
					position++;
				}
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			/*
				Sent the elements to the next process. The size is 100. Tag will 1 for all connections.
				Neutrilize the lastelems array.
			*/
			position = 0;
			MPI_Send(lastelems, height, MPI_INT, id + 1, tag, MPI_COMM_WORLD);
			for (j = 0;j < height;j++) {
				lastelems[j] = 0;
			}

			/*
				Immediately receive the values from the next process.
				Pass them to the last row of the I.
				Then deallocate the lastelems array.

			*/
			MPI_Recv(lastelems, height, MPI_INT, id + 1, tag, MPI_COMM_WORLD, &status);
			for (j = 0;j < height;j++) {
				I[sub_height][j] = lastelems[j];
			}
			free(lastelems);
			printf("|Finished preparing and sending data for process %d|\n", id);
		}

		/*
				This part is the same as for the 0 process and is the same as is for p = 2.
		*/
		if (id == p - 1) {

			/*
				Create the I array with additional row.
				Initialize it with zeros.
				Create recv_elements to get the sent values from previous process.
				Straight away pass it to the first row of the I array so the elements
				will act as the first of it.
			*/
			I = (int**)malloc(sizeof(int*) * (sub_height + 1));
			if (I) {
				for (i = 0;i <= sub_height;i++) {
					I[i] = (int*)malloc(sizeof(int) * height);
					if (I[i] != NULL) {
						for (j = 0; j < height; j++) {
							I[i][j] = 0;
						}
					}
					else {
						printf("Malloc allocation failed. Terminating program...\n");
						MPI_Finalize();
						return 0;
					}
				}
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			int* recv_elements = (int*)malloc(sizeof(int) * height);
			if (recv_elements) {
				MPI_Recv(recv_elements, height, MPI_INT, id - 1, tag, MPI_COMM_WORLD, &status);
				for (j = 0;j < height;j++) {
					I[0][j] = recv_elements[j];
				}
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			/*
				Neutrilize the recv_elements array. Again this is not necessary.
				Get the first 100 elements of the subarray of this process.
				Send them to the previous process, as they will act as the last elements
				of it.
			*/
			position = 0;
			for (j = 0;j < height;j++) {
				recv_elements[j] = 0;
			}

			for (j = 0;j < height;j++) {
				recv_elements[j] = subarray[position];
				position++;
			}
			MPI_Send(recv_elements, height, MPI_INT, id - 1, tag, MPI_COMM_WORLD);

			/*
				Fill the I array after the first row with the values of the subarray.
				Free the recv_elements array.
				The I array is ready for the convolution part.
			*/
			position = 0;;
			for (i = 1;i <= sub_height;i++) {
				for (j = 0;j < height;j++) {
					I[i][j] = subarray[position];
					position++;
				}
			}
			free(recv_elements);

			printf("|Finished preparing and sending data for process %d|\n", id);
		}

		/*
			This part changes because the middle pack of processes are executed.
			Right away there are some differences with the other parts.
			First and foremost the I array tha is created (again seperately for each process)
			has two additional rows. The reason for this is that, the process must receive the
			last 100 elements of the previous process and also receive the first 100 elements
			that belong to the next process. To conclude the I will have 200 elements more than
			the other processes.
		*/
		if (id >= 1 && id != p - 1) {
			I = (int**)malloc(sizeof(int*) * (sub_height + 2));
			if (I) {
				for (i = 0;i < sub_height + 2;i++) {
					I[i] = (int*)malloc(sizeof(int) * height);
					if (I[i] != NULL) {
						for (j = 0;j < height;j++) {
							I[i][j] = 0;
						}
					}else {
						printf("Malloc allocation failed. Terminating program...\n");
						MPI_Finalize();
						return 0;
					}
				}
			}else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			/*
				We create two arrays. This is for convenience. Both of them have size of 100
				elements and will be used correspondingly for the previous and the next process.
			*/
			int* prev_elements, * next_elements;

			/*
				The first array, prev_elements, is associated to the previous process.
				The first step is to recv the elements with size equal to 100.
				Check malloc allocation of course.
			*/
			prev_elements = (int*)malloc(sizeof(int) * height);
			if (prev_elements) {
				MPI_Recv(prev_elements, height, MPI_INT, id - 1, tag, MPI_COMM_WORLD, &status);
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			/*
				Those received elements will be used as the first row of the I array.
				Instantly neutrilize the prev_elements with zero, pass to it the first
				100 elements of the corresponding subarray. Afterwards send them back to
				the previous process
			*/
			for (j = 0;j < height;j++) {
				I[0][j] = prev_elements[j];
				prev_elements[j] = 0;
				prev_elements[j] = subarray[position];
				position++;
			}
			position = 0;
			MPI_Send(prev_elements, height, MPI_INT, id - 1, tag, MPI_COMM_WORLD);

			/*
				In the meantime pass the subarray's values to the I array and take care not
				to meddle with the first row.
			*/
			for (i = 1;i < sub_height + 1;i++) {
				for (j = 0;j < height;j++) {
					I[i][j] = subarray[position];
					position++;
				}
			}

			/*
				Now to fix the connections with the next process.
				Create the next_elements array with size of 100 integers.
				Because this will be used to send the last elements of this process'
				last 100 elements on to the next the position variable is set to start
				in the first of those 100.
			 */
			next_elements = (int*)malloc(sizeof(int) * height);
			position = size_to_be_sent - height;

			/*
				As said above get the last 100 pieces of the subarray and send them to
				the next process. Beforhand we have checked for the malloc allocation.
				The tag component is always the same.

			*/
			if (next_elements) {
				for (j = 0;j < height;j++) {
					next_elements[j] = subarray[position];
					position++;
				}
				MPI_Send(next_elements, height, MPI_INT, id + 1, tag, MPI_COMM_WORLD);
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			/*
				Straight away recv from the next process its first 100 units.
				Pass them to the last allowed row capable of the I array.
				Now the I array is complete and ready for the convolution.
				The last steps are to deallocate the prev_elements & next_elements
				to free the memory.
			*/
			MPI_Recv(next_elements, height, MPI_INT, id + 1, tag, MPI_COMM_WORLD, &status);
			for (j = 0;j < height;j++) {
				I[sub_height + 1][j] = next_elements[j];
				next_elements[j] = 0;

			}
			free(prev_elements);
			free(next_elements);
			printf("|Finished preparing and sending data for process %d|\n", id);
		}
	}

	/*
		For the second time in this program the separation happens because of the p value
		Logically this is the case because the I array in the p>2 occasions is bigger.
	*/
	if (p == 2) {

		/*
			Create the A array for the convolution. The procedure here is the same as that of the I.
			It has the same sizes as well.Check malloc allocation. If all is normal then initialize
			it with zeros.
		*/
		A = (int**)malloc(sizeof(int*) * (sub_height + 1));
		if (A) {
			for (i = 0; i < sub_height; i++) {
				A[i] = (int*)malloc(sizeof(int) * height);
				if (A[i] != NULL) {
					for (j = 0; j < height; j++) {
						A[i][j] = 0;
					}
				}
				else {
					printf("Malloc allocation failed. Terminating program...\n");
					MPI_Finalize();
					return 0;
				}
			}
		}
		else {
			printf("Malloc allocation failed. Terminating program...\n");
			MPI_Finalize();
			return 0;
		}

		/*
			This is were the convolution occurs.The basic equation is changed in the slightest of ways.
			Because of the added rows in the padding area the dimensions are interchanged from what they
			should be based on the equation given. But there is an even more important reason for this.
			The reason is that at max, because of the way scatter works I array and ancillary the A, will
			be constructed with 51 rows. When the dimension are inversed the y variable will run until it
			is 100 resulting in segmentation fault. Also that is why x and y start at 1 and end when they
			are equal to second to last. Ignoring the added rows, but at the same time using them for the
			convolution of the others.
		*/
		for (x = 1; x < sub_height; x++) {
			for (y = 1; y < height - 1; y++) {
				for (i = -1; i < 2; i++) {
					for (j = -1; j < 2; j++) {
						A[x][y] += h[j + 1][i + 1] * I[x - i][y - j];
					}
				}
				//Check to see if there are any negative numbers produces and turn them into zero. 
				if (A[x][y] < 0) {
					A[x][y] = 0;
				}
			}
		}

		/*
			Now the gather part is starting.
			Firstly the subarray will be used to gather the elements from each process.
			To be certain that there are no mistakes initialize it with zeros while using the
			2D iteration format.
		*/
		position = 0;
		for (i = 0;i < sub_height;i++) {
			for (j = 0; j < height; j++) {
				subarray[position] = 0;
				position++;
			}
		}

		/*
			In the case of the first process the number of elements of the A are the first 5000.
			So the last 100 elements that it received are not included in them. But in the case
			of the last process inside his 5000 the first 100 are the ones it retreived. So it is
			very important to not include them into the subarray, because if that was the case we
			would lose the last 100 elements. Therefore if the last process is executed we pass
			the first 100 elements and the transfer the data to the subarray.

		*/
		pos_counter = 0;
		position = 0;
		for (i = 0; i < sub_height; i++) {
			for (j = 0; j < height;j++) {
				if (id == 1) {
					pos_counter++;
					if (pos_counter <= height) {
						continue;
					}
					subarray[position] = A[i][j];
					position++;
				}
				else {
					subarray[position] = A[i][j];
					position++;

				}
			}
		}
		printf("|Finished Convolution for process %d|\n", id);
		/*
			Convolution finished for p = 2.
		*/

	}
	else if (p > 2) {
		/*
			Because the I array is bigger for the middle pack of processes here we have an extra separation.
			The first and last proesses have the exact same execution as for p = 2. So there is no point in
			analyzing it for the second time.
		*/
		if (id == 0 || id == p - 1) {
			A = (int**)malloc(sizeof(int*) * (sub_height + 1));
			if (A) {
				for (i = 0; i < sub_height; i++) {
					A[i] = (int*)malloc(sizeof(int) * height);
					if (A[i] != NULL) {
						for (j = 0; j < height; j++) {
							A[i][j] = 0;
						}
					}
					else {
						printf("Malloc allocation failed. Terminating program...\n");
						MPI_Finalize();
						return 0;
					}
				}
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			for (x = 1; x < sub_height; x++) {
				for (y = 1; y < height - 1; y++) {
					for (i = -1; i < 2; i++) {
						for (j = -1; j < 2; j++) {
							A[x][y] += h[j + 1][i + 1] * I[x - i][y - j];
						}
					}
					if (A[x][y] < 0) {
						A[x][y] = 0;
					}
				}
			}

			/*
			Now the gather part is starting.
			Firstly the subarray will be used to gather the elements from each process.
			To be certain that there are no mistakes initialize it with zeros while using the
			2D iteration format.
			*/
			position = 0;
			for (i = 0;i < sub_height;i++) {
				for (j = 0; j < height; j++) {
					subarray[position] = 0;
					position++;
				}
			}

			pos_counter = 0;
			position = 0;

			for (i = 0; i < sub_height; i++) {
				for (j = 0; j < height;j++) {
					pos_counter++;
					if (pos_counter <= height && id == p - 1) {
						continue;
					}
					subarray[position] = A[i][j];
					position++;
				}
			}
			printf("|Finished Convolution for process %d|\n", id);

		}
		else {

			/*
				Here is the main difference from p = 2.
				The A array just like the I gets two additional rows.
				Check malloc allocation and then initialize it with zeros.
			*/
			A = (int**)malloc(sizeof(int*) * (sub_height + 2));
			if (A) {
				for (i = 0; i <= sub_height + 1; i++) {
					A[i] = (int*)malloc(sizeof(int) * height);
					if (A[i] != NULL) {
						for (j = 0; j < height; j++) {
							A[i][j] = 0;
						}
					}
					else {
						printf("Malloc allocation failed. Terminating program...\n");
						MPI_Finalize();
						return 0;
					}
				}
			}
			else {
				printf("Malloc allocation failed. Terminating program...\n");
				MPI_Finalize();
				return 0;
			}

			/*
				The convolution only changes in the x department where the iteration will be executed
				for one additional row. The I remember, contains elements given to it for the first row
				and for the last. So it has to make the convolution happen in all the in between units.
				That is why this part runs for one more row.
			*/
			for (x = 1; x < sub_height + 1; x++) {
				for (y = 1; y < height - 1; y++) {
					for (i = -1; i < 2; i++) {
						for (j = -1; j < 2; j++) {
							A[x][y] += h[j + 1][i + 1] * I[x - i][y - j];
						}
					}
					//Check for negative values and turn them to zero.
					if (A[x][y] < 0) {
						A[x][y] = 0;
					}

				}
			}

			/*
			Now the gather part is starting.
			Firstly the subarray will be used to gather the elements from each process.
			To be certain that there are no mistakes initialize it with zeros while using the
			2D iteration format.
			*/
			position = 0;
			for (i = 0;i < sub_height;i++) {
				for (j = 0; j < height; j++) {
					subarray[position] = 0;
					position++;
				}
			}

			pos_counter = 0;
			position = 0;
			/*
				For every process skip the first 100 elements and run the iteration for size to sent
				times. Therefore the subarray will get the exactammount that is convoluted.
			*/
			for (i = 0; i < sub_height + 1; i++) {
				for (j = 0; j < height;j++) {
					pos_counter++;
					if (pos_counter <= height) {
						continue;
					}
					subarray[position] = A[i][j];
					position++;

				}
			}
			printf("|Finished Convolution for process %d|\n", id);
		}
	}

	/*
		The Convolution ends for all cases. Now the writing to a file remains
		Get the execution time for each process.
	*/
	wtime = MPI_Wtime() - wtime;
	printf("|Time of execution for process %d ==> %f|\n\n", id, wtime);

	/*
		But firstly we need to gather the data from each process.
		Create a 1D array with the size of the picture.
		Check malloc allocation.
	*/
	if (id == 0) {
		gather = (int*)malloc(sizeof(int) * size_to_be_sent * p);
		if (!gather) {
			printf("Malloc allocation failed. Terminating program...\n");
			MPI_Finalize();
			return 0;
		}
	}

	/*
		This is where it all come down to. MPI_Gather collects every part - subarray and unites them
		into the gather array.
	*/
	MPI_Gather(subarray, size_to_be_sent, MPI_INT, gather, size_to_be_sent, MPI_INT, master, MPI_COMM_WORLD);

	if (id == 0) {
		/*
			Now to save them.
			Open the new file connection to the second file.
			Check to see if it openned succesfully.
			If the file does not exist it will be created.
		*/

		fpout = fopen("image_alter.bmp", "wb");
		if (fpout == NULL) {
			printf("The file cound not be openned or created.\n");
			MPI_Finalize();
			return 0;
		}

		/*
			Write the characteristics of the other file to the new one.
			The last separation of choices is here.
			If p > 4 then skip the added rows that originally happened during padding.
			Be careful to store to the file only the ammount of elements it can withhold.
			Do no forget that the file is a bmp image, so for each position, three values
			are needed. Using putc to insert one unit at a time.

		*/
		fwrite(header, sizeof(unsigned char), 54, fpout);
		position = 0;
		if (p == 8 || p == 16) {
			for (i = 0;i < height + (p - 4);i++) {
				for (j = 0; j < width; j++) {
					if (position >= (height * ((p - 4) / 2)) && position < ((height * ((p - 4) / 2)) + (height * width))) {
						putc(gather[position], fpout);
						putc(gather[position], fpout);
						putc(gather[position], fpout);

					}
					position++;
				}
			}
			/*
				If p <= 4, then input the elements normaly just like the previous question.
			*/
		}
		else {
			for (i = 0;i < height;i++) {
				for (j = 0; j < width; j++) {
					putc(gather[position], fpout);
					putc(gather[position], fpout);
					putc(gather[position], fpout);
					position++;
				}
			}
		}
		/*
			Close the connection to the file
			and deallocate all of the gather array and the columns of A an I
		*/
		fclose(fpout);
		free(gather);
		free(A);
		free(I);
		printf("|*** Program finished.To see the result open the file used to write the convoluted data. ***|\n");

	}
	MPI_Finalize();
	return 0;

}