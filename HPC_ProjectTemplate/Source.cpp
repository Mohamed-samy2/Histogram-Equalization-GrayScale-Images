#include <iostream>
#include <math.h>
#include <stdlib.h>
#include<string.h>
#include<msclr\marshal_cppstd.h>
#include <ctime>// include this header 
#include <mpi.h>
#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;


int* inputImage(int* w, int* h, System::String^ imagePath) //put the size of image in w & h
{
	int* input;


	int OriginalImageWidth, OriginalImageHeight;

	//*********************************************************Read Image and save it to local arrayss*************************	
	//Read Image and save it to local arrayss

	System::Drawing::Bitmap BM(imagePath);

	OriginalImageWidth = BM.Width;
	OriginalImageHeight = BM.Height;
	*w = BM.Width;
	*h = BM.Height;
	int *Red = new int[BM.Height * BM.Width];
	int *Green = new int[BM.Height * BM.Width];
	int *Blue = new int[BM.Height * BM.Width];
	input = new int[BM.Height*BM.Width];
	for (int i = 0; i < BM.Height; i++)
	{
		for (int j = 0; j < BM.Width; j++)
		{
			System::Drawing::Color c = BM.GetPixel(j, i);

			Red[i * BM.Width + j] = c.R;
			Blue[i * BM.Width + j] = c.B;
			Green[i * BM.Width + j] = c.G;

			input[i*BM.Width + j] = ((c.R + c.B + c.G) / 3); //gray scale value equals the average of RGB values

		}

	}
	return input;
}


void createImage(int* image, int width, int height, int index)
{
	System::Drawing::Bitmap MyNewImage(width, height);


	for (int i = 0; i < MyNewImage.Height; i++)
	{
		for (int j = 0; j < MyNewImage.Width; j++)
		{
			//i * OriginalImageWidth + j
			if (image[i*width + j] < 0)
			{
				image[i*width + j] = 0;
			}
			if (image[i*width + j] > 255)
			{
				image[i*width + j] = 255;
			}
			System::Drawing::Color c = System::Drawing::Color::FromArgb(image[i*MyNewImage.Width + j], image[i*MyNewImage.Width + j], image[i*MyNewImage.Width + j]);
			MyNewImage.SetPixel(j, i, c);
		}
	}
	MyNewImage.Save("..//Data//Output//outputRes" + index + ".png");
	cout << "result Image Saved " << index << endl;
}


int main()
{
	int ImageWidth = 4, ImageHeight = 4;

	int start_s, stop_s, TotalTime = 0;

	System::String^ imagePath;
	std::string img;
	img = "..//Data//Input//test.png";

	imagePath = marshal_as<System::String^>(img);


	MPI_Init(NULL, NULL);
	int size, rank;
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	int* imageData=NULL;
	int* new_image = NULL;
	/// Reading image

	if (rank == 0) {


		imageData = inputImage(&ImageWidth, &ImageHeight, imagePath);
		new_image = new int[ImageHeight * ImageWidth];
		start_s = clock();


	}

	
	MPI_Bcast(&ImageHeight, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&ImageWidth, 1, MPI_INT, 0, MPI_COMM_WORLD);


	/// Intialize constants and variables
	const int histogramsize = 256;
	const int scale_value = 255;

	const int scatter_size = (ImageHeight * ImageWidth) / size;


	float global_cdf[histogramsize] = {};
	float freq_arr[histogramsize] = {};

	float* global_pdf = new float[histogramsize];
	float global_freq_arr[histogramsize];


	///  Broadcast the used variables for all processors

	MPI_Bcast(freq_arr, histogramsize, MPI_FLOAT, 0, MPI_COMM_WORLD);
	
	int image_remainder = (ImageHeight*ImageWidth) % size;

	

	///  Scatter image to all processors


	int* local_image = new int[scatter_size];


	
	//MPI_Scatterv(imageData, count, displacement, MPI_INT, local_image, count[rank], MPI_INT, 0, MPI_COMM_WORLD);
	

	



	MPI_Scatter(imageData, scatter_size, MPI_INT, local_image, scatter_size, MPI_INT, 0, MPI_COMM_WORLD);
	//MPI_Scatter(imageData, scatter_size, MPI_INT, local_image, scatter_size, MPI_INT, 0, MPI_COMM_WORLD);




	///  Compute frequency for each pixel
	for (int i = 0; i < scatter_size; i++) {
		freq_arr[local_image[i]] += 1;
	}

	if (rank == 0 && size != 1) {


		for (int i = (ImageHeight*ImageWidth)-image_remainder; i < ImageHeight * ImageWidth; i++) {

			freq_arr[imageData[i]] += 1;

		}


	}




	//if (rank == size - 1 && size != 1 ) {
	//	int* remindar_image = new int[image_remainder];

	//	MPI_Status stat;
	//	MPI_Recv(remindar_image, image_remainder, MPI_INT, 0, 0, MPI_COMM_WORLD, &stat);

	//	for (int i = 0; i < image_remainder; i++) {
	//		freq_arr[remindar_image[i]] += 1;
	//	}

	//	free(remindar_image);
	//}



	///  Collect the frequencies from all processor 

	MPI_Reduce(freq_arr, global_freq_arr, histogramsize, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);


	float* local_pdf = new float[histogramsize/size];

	int remainder = histogramsize % size;




	MPI_Scatter(global_freq_arr, histogramsize/size, MPI_FLOAT, local_pdf, histogramsize / size, MPI_FLOAT, 0, MPI_COMM_WORLD);





	for (int i = 0; i < (histogramsize / size); i++) {
		local_pdf[i] = local_pdf[i] / (ImageHeight * ImageWidth);
	}


	if (rank == 0 && size != 1) {

		for (int i = histogramsize-remainder; i < histogramsize; i++) {

			global_pdf[i] = global_freq_arr[i] / (ImageHeight * ImageWidth);

		}
	}




	/*if (rank == size - 1 && size != 1) {

		float* remindar_pdf = new float[remainder];

		MPI_Status stat;
		MPI_Recv(remindar_pdf, remainder, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, &stat);

		for (int i = 0; i < (histogramsize / size); i++) {
			remindar_pdf[i] = remindar_pdf[i] / (ImageHeight * ImageWidth);
		}

		MPI_Send(remindar_pdf, remainder, MPI_FLOAT, 0, 0, MPI_COMM_WORLD);
	}*/


	/*if (rank == 0 && size != 1) {
		MPI_Status stat;
		MPI_Recv(&global_pdf[histogramsize - remainder], remainder, MPI_FLOAT, size - 1, 0, MPI_COMM_WORLD, &stat);

	}*/




	MPI_Gather(local_pdf, histogramsize / size, MPI_FLOAT, global_pdf, histogramsize / size, MPI_FLOAT, 0, MPI_COMM_WORLD);







	//int* recv_count = new int[size];
	//int* displacement = new int[size];

	///  Intialize the receive count and displacement



	//for (int i = 0; i < size; i++) {
	//	if (i == size - 1) {
	//		count[i] = (histogramsize / size) + remainder;
	//		displacement[i] = i * (histogramsize / size);
	//	}
	//	else {
	//		count[i] = (histogramsize / size);
	//		displacement[i] = i * (histogramsize / size);
	//	}
	//}


	/////  Calculate the PDF for each pixel

	//for (int i = displacement[rank]; i < displacement[rank] + count[rank]; i++) {
	//	if (i < histogramsize) {
	//		global_freq_arr[i] = global_freq_arr[i] / (ImageHeight * ImageWidth);
	//	}
	//}

	/////  Collect all the calculated PDF's
	//MPI_Gatherv(global_freq_arr + displacement[rank], count[rank], MPI_FLOAT,
	//	global_pdf, count, displacement, MPI_FLOAT, 0, MPI_COMM_WORLD);

	//
	//
	// 
	// 
	// 
	/////  Calculate the CDF and multiply it by our range 

	if (rank == 0) {

		for (int i = 0; i < histogramsize; i++) {
			if (i) {
				global_cdf[i] = global_cdf[i - 1] + global_pdf[i];
			}
			else {
				global_cdf[i] = global_pdf[i];
			}
		}

		for (int i = 0; i < histogramsize; i++) {
			global_cdf[i] = floorf(global_cdf[i] * scale_value);

			
		}


	}



	/////  Broadcast the CDF for all processors
	MPI_Bcast(global_cdf, histogramsize, MPI_FLOAT, 0, MPI_COMM_WORLD);

	


	//	for (int i = 0; i < size; i++) {
	//		if (i == size - 1) {
	//			count[i] = (scatter_size) + image_remainder;
	//			displacement[i] = i * (scatter_size);
	//		}
	//		else {
	//			count[i] = (scatter_size);
	//			displacement[i] = i * (scatter_size);
	//		}


	//	}



		// map image

		for (int i = 0; i < scatter_size; i++)
		{
			local_image[i] = global_cdf[local_image[i]];
		}



		if (rank == 0&& size!=1) {


			for (int i = (ImageHeight*ImageWidth)-image_remainder; i < ImageHeight*ImageWidth; i++) {

				new_image[i] = global_cdf[imageData[i]];

			}
		}


	/*	if (rank == size - 1 && size != 1) {
			int* remindar_image = new int[image_remainder];

			MPI_Status stat;
			MPI_Recv(remindar_image, image_remainder, MPI_INT, 0, 0, MPI_COMM_WORLD, &stat);
			
			for (int i = 0; i < image_remainder; i++) {
				remindar_image[i]=global_cdf[remindar_image[i]];
				
			}

			MPI_Send(remindar_image, image_remainder, MPI_INT, 0, 0, MPI_COMM_WORLD);


		}*/


	/////*for (int i = rank * scatter_size; i < (rank * scatter_size) + scatter_size; i++) {
	////	imageData[i] = global_cdf[imageData[i]];
	////	
	////}*/

	/*if (rank == 0 && size != 1) {
		MPI_Status stat;
		MPI_Recv(&new_image[(ImageHeight * ImageWidth) - image_remainder], image_remainder, MPI_INT, size - 1, 0, MPI_COMM_WORLD, &stat);
	}*/



	//MPI_Barrier(MPI_COMM_WORLD);

	MPI_Gather(local_image, scatter_size, MPI_INT, new_image, scatter_size, MPI_INT, 0, MPI_COMM_WORLD);

	////MPI_Gatherv(local_image, count[rank], MPI_INT, new_image, count, displacement, MPI_INT, 0, MPI_COMM_WORLD);

		

		if (rank == 0) {
		stop_s = clock();
		TotalTime += (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
		cout << "time: " << TotalTime << endl;
		createImage(new_image, ImageWidth, ImageHeight, 444);


		}



	MPI_Finalize();

	free(local_image);
	free(imageData);
	free(global_pdf);
	free(new_image);
	free(local_pdf);
	//free(count);
	//free(displacement);

	return 0;
}