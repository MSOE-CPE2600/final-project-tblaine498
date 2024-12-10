#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <bits/getopt_core.h>
#include <sys/mman.h>
#include <bits/mman-linux.h>

#define RGB_VALUE 3

static void *compute_image(void *arg);
static void *sRGB(unsigned char *row);
static void *histeq();
static void *invert(unsigned char *row);
static void *greyscale(unsigned char *row);
static void show_help();

double *nk[256] = {0};

// Basically defines a image_processing_func as a 
// function that takes in nothing and returns nothing
typedef void (*image_processing_func)(unsigned char *row);

typedef struct thread_args {
    unsigned char *data;
	int start_row;
	int end_row;
    void *func_ptr;
} thread_args;

int main(int argc, char *argv[])
{

    for (int i = 0; i < 256; i++) {
        nk[i] = malloc(sizeof(int)); // Allocate memory for each element
        if (nk[i] != NULL) {
            *nk[i] = 0; // Initialize to 0
        } else {
            // Handle memory allocation failure if needed
            fprintf(stderr, "Memory allocation failed for nk[%d]\n", i);
            exit(EXIT_FAILURE);
        }
    }

    char c;

    // These are the default configuration values used
	// if no command line arguments are given.
    char *input_name         = "airplane"; 
	char *output_name        = "output"; 
    char *enhancement        = "clrspc-sRGB";
    const char *extension    = ".ppm"; 
    int         image_width  = 512;
	int         image_height = 512;
    int         num_threads  = 1;

    // For each command line argument given,
	// override the appropriate configuration value.
    while((c = getopt(argc,argv,"W:H:t:e:i:o:h"))!=-1) {
		switch(c) 
		{
            case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 't':
				num_threads = atoi(optarg);
				break;
            case 'e':
				enhancement = optarg;
				break;   
            case 'i':
				input_name = optarg;
				break;   
			case 'o':
				output_name = optarg;
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}
    printf("-W %d -H %d -t %d -e %s -i %s -o %s\n", image_width, image_height, num_threads, 
                                                enhancement, input_name, output_name);
    // Create file pointers
    FILE *fp;
    FILE *new_fp;

    // Create path to input file
    char input_path[100];
    sprintf(input_path, "images/%s%s", input_name, extension);

    // Create path to output file
    char output_path[100];
    sprintf(output_path, "images/%s%s", output_name, extension);

    // Open files
    fp = fopen(input_path, "r");
    new_fp = fopen(output_path, "w");
    if (!fp || !new_fp)
    {
        fputs("Error opening file\n", stderr);
        exit(1);
    }

    // Array to get 1 row of image
    char line[image_width * RGB_VALUE];

    // Copy header info to the new file
    for (int i = 0; i < 4; i++)
    {
        fgets(line, sizeof(line), fp);
        fputs(line, new_fp);
    }

    // Use mmap to allocate shared memory
    // NULL - allows operating system to decide where to place memory
    // IMG_SIZE * IMG_SIZE * sizeof(unsigned char) - amount of memory needed to allocate 
    // PROT_READ | PROT_WRITE - allows processes to read and write from shared memory
    // MAP_SHARED | MAP_ANONYMOUS - parameters to create shared anonymous memory
    // -1, 0 - required parameters for MAP_ANONYMOUS
    unsigned char *image_data = mmap(NULL, image_width * image_height * RGB_VALUE * sizeof(unsigned char),
                                     PROT_READ | PROT_WRITE,
                                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (image_data == MAP_FAILED)
    {
        fputs("Memory allocation failed\n", stderr);
        fclose(fp);
        fclose(new_fp);
        return 1;
    }

    // Read the image data into shared memory
    for (int i = 0; i < image_width * image_height * RGB_VALUE; i++)
    {
        image_data[i] = fgetc(fp);
    }

    // Determine number of rows each thread must process plus extra rows due to division
    int segment_size = image_height / num_threads;
    int remainder = image_height % num_threads;
    
	// Split the iterations among mulitple threads
	pthread_t threads[num_threads];
	thread_args thread_params[num_threads];

    for (int t = 0; t < num_threads; t++)
    {
        // Add mmap to args
        thread_params[t].data = image_data;

        // Determines if extra iterations are needed based on the remainder
        int extra_iterations_threads = 0;
        if (t < remainder)
        {
            extra_iterations_threads = t;
        } 
        else 
        {
            extra_iterations_threads = remainder;
        }

        // Sets the start/end index
        thread_params[t].start_row = t * segment_size + extra_iterations_threads;
        thread_params[t].end_row = thread_params[t].start_row + segment_size - 1;

        // Add one more iteration for threads with remainder
        if (t < remainder)
        {
            thread_params[t].end_row++;
        }
        
        // Determine function enhancement on image
        if (strcmp(enhancement, "clrspc-sRGB") == 0)
        {
            thread_params[t].func_ptr = sRGB;
        }
        else if (strcmp(enhancement, "histeq") == 0)
        {
            thread_params[t].func_ptr = histeq;
        }
        else if (strcmp(enhancement, "invert") == 0)
        {
            thread_params[t].func_ptr = invert;
        }
        else if (strcmp(enhancement, "greyscale") == 0)
        {
            thread_params[t].func_ptr = greyscale;
        }
        else
        {
            //FIXME
            thread_params[t].func_ptr = sRGB;
        }

        // Creates thread
        pthread_create(&threads[t], NULL, compute_image, &thread_params[t]);
    }

    // Waits for all threads to finish
    for (int t = 0; t < num_threads; t++)
    {
        pthread_join(threads[t], NULL);
    }

    if (strcmp(enhancement, "histeq") != 0)
    {
        // Write new image data to the output file
        for (int i = 0; i < image_width * image_height * RGB_VALUE; i++)
        {
            fputc(image_data[i], new_fp);
        }
    }
    else 
    {
        int n = 0;
        for (int i = 0; i < 256; i++)
        {
            n += *nk[i];
            printf("%d: %f\n", i, *nk[i]);
        }
        printf("%d\n", n);

        for (int i = 0; i < 256; i++)
        {
            (*nk[i]) = *nk[i] / n;
            printf("%d: %f\n", i, *nk[i]);
        }

        double total = 0;
        for (int i = 0; i < 256; i++)
        {
            total += *nk[i];
            (*nk[i]) = total;
            printf("%d: %f\n", i, *nk[i]);
        }
        for (int i = 0; i < 256; i++)
        {
            (*nk[i]) = *nk[i] * 255;
            printf("%d: %f\n", i, *nk[i]);
        }
        for (int i = 0; i < 256; i++)
        {
            (*nk[i]) = round(*nk[i]);
            printf("%d: %f\n", i, *nk[i]);
        }
        unsigned char lookup[256];
        for (int i = 0; i < 256; i++) {
            lookup[i] = (unsigned char)(*nk[i]);
        }

        for (int i = thread_params->start_row; i <= thread_params->end_row; i++)
        {
            unsigned char *row = thread_params->data + (i * 512 * RGB_VALUE);
            // Apply the new values to the image
            for (int i = 0; i < 512*3; i++) {
                row[i] = lookup[row[i]];
            }
        }

        // Free the allocated memory
        for (int i = 0; i < 256; i++) {
            free(nk[i]);
        }
        // Write new image data to the output file
        for (int i = 0; i < image_width * image_height * RGB_VALUE; i++)
        {
            fputc(image_data[i], new_fp);
        }
    }

    munmap(image_data, image_width * image_height * RGB_VALUE * sizeof(unsigned char));
    fclose(fp);
    fclose(new_fp);
    return 0;

}

static void *compute_image(void *arg)
{
    // Converts null pointer to struct pointer
    thread_args *args = (thread_args *)arg;

    // Loop through each value calling the specific processing function
    for (int i = args->start_row; i <= args->end_row; i++)
    {
        // Need to cast function pointer into a 'type' the complier knows so we defined it up above
        // Complier wants to know more than just its a void pointer in order to call the function
        ((image_processing_func)args->func_ptr)(args->data + (i * 512 * RGB_VALUE));
    }
    return NULL;
}

static void *sRGB(unsigned char *row)
{
    const double a = 1.055;
    const double b = -0.055;
    const double c = 12.92; 
    const double d = 0.0031308;
    const double gamma = 1.0 / 2.4;

    for (int i = 0; i < 512 * RGB_VALUE; i++)
    {
        // Normalize to [0, 1]
        double normalized = row[i] / 255.0;

        double result;
        if (normalized < d)
        {
            result = c * normalized;
        }
        else
        {
            result = a * pow(normalized, gamma) + b;
        }

        // Convert back to regular RGB value [0, 255]
        row[i] = (unsigned char)(result * 255.0 + 0.5);
    }
    return NULL;
}

static void *histeq(unsigned char *row)
{
    for (int i = 0; i < 256; i++)
    {
        for (int j = 0; j < 512 * RGB_VALUE; j++)
        {
            if (i == row[j])
            {
                (*nk[i])++;
            }
        }
    }

    return NULL;
}

static void *invert(unsigned char *row)
{
    for (int i = 0; i < 512 * RGB_VALUE; i++)
    {
        row[i] = 256 - row[i];
    }
    return NULL;
}

static void *greyscale(unsigned char *row)
{
    for (int i = 0; i < 512 * RGB_VALUE; i++)
    {
        if (i % RGB_VALUE == 0)
        {
            row[i] = 0.299 * row[i];
        }
        else if (i % RGB_VALUE == 1)
        {
            row[i] = 0.587 * row[i];
        }
        else
        {
            row[i] = 0.114 * row[i];
            row[i-2] = row[i-2] + row[i-1] + row[i];
            row[i-1] = row[i-2];
            row[i] = row[i-2];
        }
    }
    return NULL;
}

// Show help message
void show_help()
{
	printf("Options\n");
	printf("-W <pixels>   Width of the image in pixels. (default=512)\n");
	printf("-H <pixels>   Height of the image in pixels. (default=512)\n");
	printf("-t <threads>  Number of threads used to create the transformed image. (default=1)\n");
    printf("-e <type>     Name of the enhancement to be done on image [options: histeq, invert, clrspc-sRGB, greyscale] (default=clrspc-sRGB)\n");
	printf("-i <file>     Set input file. (default=airplane.ppm)\n");
    printf("-o <file>     Set output file. (default=output.ppm)\n");
	printf("-h            Show this help text.\n");
    printf("Note: Do not include file extensions such as .ppm\n");
    printf("Note: Only images in the 'images' folder will work\n");
	printf("\nSome examples are:\n");
	printf("output -t 2 -e invert -i sailboat\n");
	printf("output -t 10 -o new_image\n\n");
}