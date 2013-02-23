/*
Copyright © 2013 Henrik Andersson

This file is part of FLARE.

FLARE is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

FLARE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
FLARE.  If not, see http://www.gnu.org/licenses/
*/

/**
 * class MenuNPCActions
 */
#include <sstream>
#include "Menu.h"
#include "MenuNPCActions.h"
#include "NPC.h"
#include "Settings.h"
#include "SharedResources.h"
#include "SDL_gfxBlitFunc.h"

#define SEPARATOR_HEIGHT 2
#define ITEM_SPACING 2
#define MENU_BORDER 8

using namespace std;

class Action {
public:
	Action(std::string _id = "", std::string _label="") : label(NULL) {
		id = _id;
		if (id != "") {
			label = new WidgetLabel();
			label->set(_label);
		}
	}

	Action(const Action &r) : label(NULL) {
		id = r.id;
		if (id != "") {
			label = new WidgetLabel();
			label->set(r.label->get());
		}
	}

	virtual ~Action() { delete label; }

	std::string id;
	WidgetLabel *label;
	SDL_Rect rect;
};

MenuNPCActions::MenuNPCActions()
	: Menu()
	, npc(NULL)
	, is_selected(false)
	, current_action(-1)
	, action_menu(NULL)
{
  normal_item_color.r = 0xd0;
  normal_item_color.g = 0xd0;
  normal_item_color.b = 0xd0;

  hilight_item_color.r = 0xff;
  hilight_item_color.g = 0xff;
  hilight_item_color.b = 0xff;
}

void MenuNPCActions::update() {
	if (action_menu)
		SDL_FreeSurface(action_menu);

	/* get max width and height of action menu */
	int w = 0, h = 0; 
	for(size_t i=0; i<npc_actions.size(); i++) {
		h += ITEM_SPACING;
		if (npc_actions[i].label) {
			w = max((int)npc_actions[i].label->bounds.w, w);
			h += npc_actions[i].label->bounds.h;
		}
		else
			h += SEPARATOR_HEIGHT;

		h += ITEM_SPACING;
	}

	/* set action menu position */
	window_area.x = VIEW_W_HALF - (w / 2);;
	window_area.y = 40;
	window_area.w = w;
	window_area.h = h;

	/* update all action menu items */
	int yoffs = MENU_BORDER;
	for(size_t i=0; i<npc_actions.size(); i++) {
		npc_actions[i].rect.x = window_area.x + MENU_BORDER;
		npc_actions[i].rect.y = window_area.y + yoffs;
		npc_actions[i].rect.w = w;
		
		if (npc_actions[i].label) {
			npc_actions[i].rect.h = npc_actions[i].label->bounds.h + (ITEM_SPACING*2);

			if (i == current_action) {
			  npc_actions[i].label->set(MENU_BORDER + (w/2),
						    yoffs + (npc_actions[i].rect.h/2) , 
						    JUSTIFY_CENTER, VALIGN_CENTER, 
						    npc_actions[i].label->get(), hilight_item_color);
			} else {
			  npc_actions[i].label->set(MENU_BORDER + (w/2), 
						    yoffs + (npc_actions[i].rect.h/2),
						    JUSTIFY_CENTER, VALIGN_CENTER,
						    npc_actions[i].label->get(), normal_item_color);
			}

		}
		else
			npc_actions[i].rect.h = SEPARATOR_HEIGHT + (ITEM_SPACING*2);

		yoffs += npc_actions[i].rect.h;
	}

	w += (MENU_BORDER*2);
	h += (MENU_BORDER*2);

	/* render action menu surface */
	action_menu = createAlphaSurface(w,h);
	Uint32 bg = SDL_MapRGBA(action_menu->format, 0, 0, 0, 0xd0);
	SDL_FillRect(action_menu, NULL, bg);

	for(size_t i=0; i<npc_actions.size(); i++) {
	  if (npc_actions[i].label) {
		  npc_actions[i].label->render(action_menu);
	  }
	}

}

void MenuNPCActions::setNPC(NPC *pnpc) {

	// clear actions menu
	npc_actions.clear();

	// reset states
	is_selected = false;
	topics = 0;
	first_dialog_node = -1;

	npc = pnpc;

	if (npc == NULL)
		return;

	// reset selection
	dialog_selected = vendor_selected = cancel_selected = false;
	
	/* enumerate available dialog topics */
	std::vector<int> nodes;
	npc->getDialogNodes(nodes);
	for (int i = (int)nodes.size() - 1; i >= 0; i--) {

		std::string topic = npc->getDialogTopic(nodes[i]);
		if (topic.length() == 0)
			continue;

		if (first_dialog_node == -1)
			first_dialog_node = nodes[i];

		stringstream ss;
		ss.str("");
		ss << "id_dialog_" << nodes[i];

		npc_actions.push_back(Action(ss.str(), topic));
		topics++;
	}


	/* if npc is a vendor add entry */
	if (npc->vendor) {
		if (topics)
			npc_actions.push_back(Action());
		npc_actions.push_back(Action("id_vendor", "Shop"));
	}

	npc_actions.push_back(Action());
	npc_actions.push_back(Action("id_cancel", "Cancel"));
       
	/* if npc is not a vendor and only one topic is
	 available select the topic automatically */
	if (!npc->vendor && topics == 1) {
		dialog_selected = true;
		selected_dialog_node = first_dialog_node;
		is_selected = true;
		return;
	}

	/* if there is no dialogs and npc is a vendor set
	 vendor_selected automatically */
	if (npc->vendor && topics == 0) {
		vendor_selected = true;
		is_selected = true;
		return;
	}

	update();

}

bool MenuNPCActions::selection() {
	return is_selected;
}

void MenuNPCActions::logic() {
	if (!visible) return;

	if (inpt->lock[MAIN1])
		return;

	/* get action under mouse */
	bool got_action = false;
	for (size_t i=0; i<npc_actions.size(); i++) {
		
		if (!isWithin(npc_actions[i].rect, inpt->mouse))
			continue;

		got_action = true;

		if (current_action != i) {
			current_action = i;
			update();
		}
		
		break;
	}

	/* if we dont have an action under mouse skip main1 check */
	if (!got_action) {
		current_action = -1;
		return;
	}
	
	/* is main1 pressed */
	if (inpt->pressing[MAIN1]) {
		inpt->lock[MAIN1] = true;

	       
		if (npc_actions[current_action].label == NULL)
			return;
		else if (npc_actions[current_action].id == "id_cancel")
			cancel_selected = true;

		else if (npc_actions[current_action].id == "id_vendor")
			vendor_selected = true;

		else if (npc_actions[current_action].id.compare("id_dialog_")) {
			dialog_selected = true;
			std::stringstream ss;
			std::string tmp(10,' ');
			ss.str(npc_actions[current_action].id);
			ss.read(&tmp[0], 10);
			ss >> selected_dialog_node;
		}

		is_selected = true;
		visible = false;
	}

}

void MenuNPCActions::render() {
	if (!visible) return;
	
	if (!action_menu) return;

	SDL_BlitSurface(action_menu, NULL, screen, &window_area);

}

MenuNPCActions::~MenuNPCActions() {
}

