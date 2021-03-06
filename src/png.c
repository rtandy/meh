#include <stdlib.h>
#include <png.h>

#include "meh.h"


struct png_t{
	struct image img;
	FILE *f;
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info;
	int numpasses;
};

static int ispng(FILE *f){
	unsigned char buf[8];
	if(fread(buf, 1, 8, f) != 8)
		return 0;
	return png_check_sig(buf, 8);
}

struct image *png_open(FILE *f){
	struct png_t *p;

	rewind(f);
	if(!ispng(f))
		return NULL;

	p = malloc(sizeof(struct png_t));
	if((p->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) == NULL){
		free(p);
		return NULL;
	}
	if((p->info_ptr = png_create_info_struct(p->png_ptr)) == NULL){
		png_destroy_read_struct(&p->png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		free(p);
		return NULL;
	}
	if((p->end_info = png_create_info_struct(p->png_ptr)) == NULL){
		png_destroy_read_struct(&p->png_ptr, &p->info_ptr, (png_infopp)NULL);
		free(p);
		return NULL;
	}
	if(setjmp(png_jmpbuf(p->png_ptr))){
		png_destroy_read_struct(&p->png_ptr, &p->info_ptr, &p->end_info);
		free(p);
		return NULL;
	}

	p->f = f;
	rewind(f);
	png_init_io(p->png_ptr, f);

	png_read_info(p->png_ptr, p->info_ptr);

	p->img.bufwidth = png_get_image_width(p->png_ptr, p->info_ptr);
	p->img.bufheight = png_get_image_height(p->png_ptr, p->info_ptr);

	p->img.fmt = &libpng;

	return (struct image *)p;
}

int png_read(struct image *img){
	unsigned int y;
	png_bytepp row_pointers;
	struct png_t *p = (struct png_t *)img;

	if(setjmp(png_jmpbuf(p->png_ptr))){
		png_destroy_read_struct(&p->png_ptr, &p->info_ptr, &p->end_info);
		return 1;
	}

	{
		int color_type = png_get_color_type(p->png_ptr, p->info_ptr);
		int bit_depth = png_get_bit_depth(p->png_ptr, p->info_ptr);
		if (color_type == PNG_COLOR_TYPE_PALETTE)
			png_set_expand(p->png_ptr);
		if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
			png_set_expand(p->png_ptr);
		if (png_get_valid(p->png_ptr, p->info_ptr, PNG_INFO_tRNS))
			png_set_expand(p->png_ptr);
		if (bit_depth == 16)
			png_set_strip_16(p->png_ptr);
		if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
			png_set_gray_to_rgb(p->png_ptr);
		//png_set_strip_alpha(p->png_ptr);

		png_color_16 my_background = {.red = 0xff, .green = 0xff, .blue = 0xff};
		png_color_16p image_background;

		if(png_get_bKGD(p->png_ptr, p->info_ptr, &image_background))
			png_set_background(p->png_ptr, image_background, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
		else
			png_set_background(p->png_ptr, &my_background, PNG_BACKGROUND_GAMMA_SCREEN, 2, 1.0);

		if(png_get_interlace_type(p->png_ptr, p->info_ptr) == PNG_INTERLACE_ADAM7)
			p->numpasses = png_set_interlace_handling(p->png_ptr);
		else
			p->numpasses = 1;
		png_read_update_info(p->png_ptr, p->info_ptr);
	}

	row_pointers = (png_bytepp)malloc(img->bufheight * sizeof(png_bytep));
	for(y = 0; y < img->bufheight; y++)
		row_pointers[y] = img->buf + y * img->bufwidth * 3;

	png_read_image(p->png_ptr, row_pointers);
	free(row_pointers);

	img->state |= LOADED | SLOWLOADED;

	return 0;
}

void png_close(struct image *img){
	struct png_t *p = (struct png_t *)img;
	png_destroy_read_struct(&p->png_ptr, &p->info_ptr, &p->end_info);
	fclose(p->f);
}

struct imageformat libpng = {
	png_open,
	NULL,
	png_read,
	png_close
};


