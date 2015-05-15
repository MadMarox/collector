
#include <iostream>
#include <string>

#include <SDL.h>

#include <collector.h>
#include <SDL_utils.h> //point_in_rect()
#include <filestore/types.h>
#include <display/cli.h>
#include <display/subtags.h>
#include <display/info.h>
#include <display/grid.h>
#include <display/thumbs.h>
#include <display/display.h>





Display::Display(Selection* init_selection)
{
	state.selection = init_selection;

	//create the main components, with references to the displays state
	cli.display     = new CLI(&state);
	subtags.display = new Subtags(&state);
	info.display    = new Info(&state);
	grid.display    = new Grid(&state);
	thumbs.display  = new Thumbs(&state);

	//trigger the initial layout
	resize();
}

Display::~Display()
{
	delete cli.display;
	delete subtags.display;
	delete info.display;
	delete grid.display;
	delete thumbs.display;
}

void Display::render()
{
	render_child(thumbs);
	render_child(grid);
	render_child(info);
	render_child(subtags);
	render_child(cli);

	sdl->reset_viewport();
}

void Display::render_child(Child& child)
{
	if(child.display->is_dirty())
	{
		sdl->set_viewport(child.rect);
		child.display->render();
	}
}

void Display::resize()
{
	SDL_Point window = sdl->window_size();
	SDL_Point middle = {
		(window.x > 0) ? (window.x / 2) : 0,
		(window.y > 0) ? (window.y / 2) : 0,
	};

	/*
		Layout information
		Some day, this should be user configurable
	*/

	cli.rect = {
		0,
		0,
		window.x,
		CLI_H
	};

	subtags.rect = {
		0,
		CLI_H,
		window.x,
		CLI_H
	};

	grid.rect = {
		0,
		subtags.rect.y + subtags.rect.h,
		window.x,
		middle.y - (CLI_H * 2)
	};

	thumbs.rect = {
		0,
		middle.y,
		window.x,
		middle.y - CLI_H
	};

	info.rect = {
		0,
		window.y - CLI_H,
		window.x,
		CLI_H
	};

	resize_child(thumbs);
	resize_child(grid);
	resize_child(info);
	resize_child(subtags);
	resize_child(cli);
}

void Display::resize_child(Child& child)
{
	sdl->set_viewport(child.rect);
	child.display->resize();
	child.display->limit_scroll();
}

void Display::request_render(Uint32 component)
{
	if(component == RENDER_THUMBS)
		thumbs.display->mark_dirty();
}

void Display::on_key(SDL_KeyboardEvent &e)
{
	switch(e.keysym.sym)
	{
		case SDLK_ESCAPE:
			sdl->quit();
			break;
		case SDLK_RETURN:
			state.selection->export_();
			break;
		case SDLK_PAGEUP:
			if(current != NULL)
				current->display->pageup();
			break;
		case SDLK_PAGEDOWN:
			if(current != NULL)
				current->display->pagedown();
			break;
		default:
			cli.display->on_key(e);
			send_selector();
			break;
	}
}

void Display::on_text(SDL_TextInputEvent &e)
{
	cli.display->on_text(e);
	send_selector();
}

void Display::on_wheel(SDL_MouseWheelEvent &e)
{
	if(current != NULL)
		current->display->on_wheel(e);		
}

void Display::on_motion(SDL_MouseMotionEvent &e)
{
	SDL_Point p = { e.x, e.y };
	if(point_in_rect(&p, &grid.rect))
		current = &grid;
	else if(point_in_rect(&p, &thumbs.rect))
		current = &thumbs;
	else
		current = NULL;

	if(current != NULL)
	{
		//adjust mouse coordinates to that of the selected viewport
		e.x -= current->rect.x;
		e.y -= current->rect.y;
		current->display->on_motion(e);
	}
}

void Display::on_click(SDL_MouseButtonEvent &e)
{
	if(current != NULL)
	{
		//adjust mouse coordinates to that of the selected viewport
		e.x -= current->rect.x;
		e.y -= current->rect.y;
		current->display->on_click(e);
		send_selector();
	}
}

void Display::on_selection(Selection* s)
{
	//replace the current selection
	state.replace_selection(s);

	//let the components update themselves
	cli.display->on_selection();
	subtags.display->on_selection();
	info.display->on_selection();
	grid.display->on_selection();

	//the new selection's layout must be computed
	/*
		TODO:
		This should really be moved to the Thumbs class,
		but the viewport must be set by this Display manager.
		Consider using a dirty state for layout
		(as well as the one for graphics).
	*/
	resize_child(thumbs); 
	thumbs.display->on_selection();
}

void Display::on_state_change()
{
	info.display->on_state_change();
}

void Display::send_selector()
{
	Selector* selector = new Selector;

	//load the current state from the UI (include/exclude table)
	state.fill_selector(selector);

	//ask the various components for their data (tag selections)
	cli.display->fill_selector(selector);

	//send to SDL event queue
	sdl->submit(SELECTOR, (void*) selector);
}
