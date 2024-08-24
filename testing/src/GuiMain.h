#pragma once


using drawcallback = void(void*);

int GuiMain(drawcallback drawfunction, void* obj_ptr);
