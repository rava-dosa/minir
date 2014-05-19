//#define _XOPEN_SOURCE 500 //strdup and realpath demands this
#define _GNU_SOURCE //strdup, realpath, asprintf
#define _strdup strdup //and windows is being windows as usual
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "host.h"

#ifndef NO_ANON_UNION_STRUCT//For crappy compilers.
#define UNION_BEGIN union {
#define UNION_END };
#define STRUCT_BEGIN struct {
#define STRUCT_END };
#else
#define UNION_BEGIN
#define UNION_END
#define STRUCT_BEGIN
#define STRUCT_END
#endif

#define STATIC_ASSERT(cond, name) extern char name[(cond)?1:-1]
#define STATIC_ASSERT_CAN_EVALUATE(cond, name) extern char name[(sizeof(cond)>=0)?1:-1]

#include "window.h"

void         //delete
window_firstrun//this
();             //one

//uncomment this to ensure they're all removed - remember to make clean
//#define window_firstrun %%%

//None of these interfaces should be instantiated and then passed around. Instead, they should be
// the first member of another, backend-specific, structure, and this structure should then be
// casted to the desired return value; the implementation can then cast it back. This way, they can
// keep state. (Exception: The None interfaces have no state, and are not backend-specific.)
//All functions taking a 'this' parameter must be called like iface->frobnicate(iface, foo, bar).
//If a function is called on anything except itself, results are undefined.
//Results are undefined if any interface is used in any way other than passing around pointers, and
// calling the functions. Don't copy them, don't cast them, don't compare them, don't access the
// functions other than to call them, and absolutely don't modify them.
//If an interface specifies cases where only a subset of the functions may be called, free() is
// always allowed, unless it specifically mentions free().
//However, if any interface defines a callback, free() is banned while inside this callback, while
// other functions are allowed. Other instances of the same interface may be used and freed, and
// other interfaces may be called.
//If an interface defines a function to set some state, and a callback for when this state changes,
// calling that function will not trigger the state callback.
//No interface may be used from multiple threads, unless otherwise specified; They must be used only
// by the one creating them. However, it is safe for any thread to create an interface.
//Don't depend on any pointer being unique; for example, the None interfaces are static. However,
// even if they are (potentially) non-unique, following the instructed method to free them is safe;
// either they're owned by the one one giving them to you, or their free() handlers are empty, or
// they could even be refcounted.
//"Implementation" means the implementation of the interfaces; the blocks of code that are called
// when a function is called on an interface.
//"User" means the one using the interfaces. Some interface implementations are users of other
// interfaces; for example, an implementation of libretro is also the user of a a dylib.
//An implementation may, at its sole discretion, choose to define any implementation of undefined
// behaviour. After all, any result, including something well defined, is a valid interpretation of
// undefined behaviour. The user may, of course, not rely on that.
//Any function that starts with an underscore may only be called by the module that implements that
// function. ("Module" is defined as "anything whose compilation is controlled by the same #ifdef,
// or the file implementing an interface, whichever makes sense"; for example, window-win32-* is the
// same module.) The arguments and return values of these private functions may change meaning
// between modules, and the functions are not guaranteed to exist at all. (For example, one of the
// three _window_init_* is redundant.)

//This file, and many other parts of minir, uses a weird mix between Windows- and Linux-style
// filenames and paths. This is intentional; the author prefers Linux-style paths and directory
// structures, but Windows file extensions. .exe is less ambigous than no extension, and Windows'
// insistence on overloading the escape character is irritating. The rest is personal preference.



struct video;
struct audio;
struct inputkb;
struct inputmouse;
struct inputjoy;
struct inputmapper;
struct dylib;
struct mutex;
struct rewindstack;
struct libretro;
struct libretroinput;



struct video {
	//Initializes the video system. It will draw on the windowhandle given during creation, at the given bit depth.
	//The user guarantees that the window is size screen_width*screen_height when this is called, and at
	// every subsequent call to draw(). If the window is resized, reinit() (or free()) must be called again.
	//The bit depths may be 32 (XRGB8888), 16 (RGB565), or 15 (0RGB1555).
	void (*reinit)(struct video * this, unsigned int screen_width, unsigned int screen_height, unsigned int depth, double fps);
	
	//Draws the given data. Size doesn't need to be same as above; if it isn't, nearest neighbor scaling will be used.
	//pitch is how many bytes to go forward to reach the next scanline.
	//If data is NULL, the last frame is redrawn, and other arguments are ignored. It will still wait for vsync.
	void (*draw)(struct video * this, unsigned int width, unsigned int height, const void * data, unsigned int pitch);
	
	//Toggles vsync; that is, whether draw() should wait for vblank before doing its stuff and
	// returning. Defaults to on; does not change on reinit().
	//Returns the previous state, if syncing is possible; otherwise, returns an undefined value.
	bool (*set_sync)(struct video * this, bool sync);
	
	//Whether vsync can be enabled on this item.
	bool (*has_sync)(struct video * this);
	
	//Returns the last frame drawn.
	//If this video driver doesn't support this, or if there is no previous frame, returns 0,0,NULL,0,16.
	bool (*repeat_frame)(struct video * this, unsigned int * width, unsigned int * height,
	                                          const void * * data, unsigned int * pitch, unsigned int * bpp);
	
	//Deletes the structure.
	void (*free)(struct video * this);
};

//This returns everything that's compiled in, but some may have runtime requirements that are not
// met. Try them in order until one works. It is guaranteed that at least one of them can
// successfully be created, but this one may not necessarily be useful.
const char * const * video_supported_backends();
struct video * video_create(const char * backend, uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                            unsigned int depth, double fps);

//It's better to use inputkb_create() and let it take care of creating the right input handlers. Otherwise, you're surrendering platform independence.
#ifdef VIDEO_OPENGL
struct video * video_create_opengl(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                   unsigned int depth, double fps);
#endif
#ifdef VIDEO_D3D9
struct video * video_create_d3d9(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                   unsigned int depth, double fps);
#endif
#ifdef VIDEO_GDI
struct video * video_create_gdi(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                unsigned int depth, double fps);
#endif
#ifdef VIDEO_XSHM
struct video * video_create_xshm(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                 unsigned int depth, double fps);
#endif
struct video * video_create_none(uintptr_t windowhandle, unsigned int screen_width, unsigned int screen_height,
                                 unsigned int depth, double fps);

//It would be cleaner if this one wrapped a struct video, but that'd make it use the video structure
// from another thread than what created it, which isn't really feasible.
struct video * video_create_thread(const char * backend, uintptr_t windowhandle,
                                   unsigned int screen_width, unsigned int screen_height,
                                   unsigned int depth, double fps);



struct audio {
	//Plays the given samples. They're interleaved left then right then left then right; one pair is
	// one frame.
	void (*render)(struct audio * this, unsigned int numframes, const int16_t * samples);
	
	//Clears out the sound buffer, silencing the buffer until the next call to render(). It is
	// implementation defined whether what's already in the buffer is played.
	void (*clear)(struct audio * this);
	
	//It is implementation defined whether doing this will drop whatever is currently in the sound buffer.
	void (*set_samplerate)(struct audio * this, double samplerate);
	void (*set_latency)(struct audio * this, double latency);
	
	//Toggles synchronization, that is, whether render() should wait or drop some samples if there's
	// insufficient freespace in the internal sound buffer. Defaults to wait.
	void (*set_sync)(struct audio * this, bool sync);
	
	bool (*has_sync)(struct audio * this);
	
	//Deletes the structure.
	void (*free)(struct audio * this);
};

const char * const * audio_supported_backends();
struct audio * audio_create(const char * backend, uintptr_t windowhandle, double samplerate, double latency);

//Should be used only within audio_create.
#ifdef AUDIO_PULSEAUDIO
struct audio * audio_create_pulseaudio(uintptr_t windowhandle, double samplerate, double latency);
#endif
#ifdef AUDIO_DIRECTSOUND
struct audio * audio_create_directsound(uintptr_t windowhandle, double samplerate, double latency);
#endif
struct audio * audio_create_none(uintptr_t windowhandle, double samplerate, double latency);



struct inputkb {
	void (*set_callbacks)(struct inputkb * this,
	                      //notifications:
	                      //- I lost track of the keyboards, you should forget all keys
	                      //- here's the current state, but don't count them as recently pressed
	                      //- number of keyboards has changed (clamp to 0-32)
	                      //- must also tell the size of a keyboard (clamp to 0-1024), though this is constant, so it can be a normal function
	                      void (*key_cb)(struct inputkb * subject,
	                                     unsigned int keyboard, unsigned int scancode, unsigned int libretrocode, 
	                                     bool down, void* userdata),
	                      void* userdata);
	
	void (*poll)(struct inputkb * this);
	
	void (*free)(struct inputkb * this);
};

const char * const * inputkb_supported_backends();
struct inputkb * inputkb_create(const char * backend, uintptr_t windowhandle);

//Linux drivers:
#ifdef INPUT_X11
struct inputkb * _inputkb_create_x11(uintptr_t windowhandle);
#endif
#ifdef INPUT_X11_XINPUT2
struct inputkb * _inputkb_create_x11_xinput2(uintptr_t windowhandle);
#endif
#ifdef INPUT_GDK
struct inputkb * _inputkb_create_gdk(uintptr_t windowhandle);
#endif
#ifdef WNDPROT_X11
//All input drivers for X11 use the same keycode translation table. This one sets up
// keyboard_num_keyboards, keyboard_num_keys, and keyboard_get_map.
//keyboard_num_keys will return 256. The driver should not override this, and may hardcode this 256 anywhere it wants.
//keyboard_num_keyboards will return 0. The driver should replace this if the input driver supports separating the keyboards.
//No deallocation needed.
//void _inputraw_x11_keyboard_create_shared(struct inputraw * this);
#endif

//Windows drivers:
#ifdef INPUT_RAWINPUT
struct inputkb * _inputkb_create_rawinput(uintptr_t windowhandle);
#endif
#ifdef INPUT_DIRECTINPUT
struct inputkb * _inputkb_create_directinput(uintptr_t windowhandle);
#endif
#ifdef WNDPROT_WINDOWS
//Windows drivers share keycode translations, too. (But they're obviously not the same as X11 key mappings.) This one acts the same way, including the 256.
//void _inputraw_windows_keyboard_create_shared(struct inputraw * this);
#endif

struct inputraw * _inputraw_create_none(uintptr_t windowhandle);

//This one translates a hardware keycode to a Libretro code. It uses the same tables as inputraw_*_keyboard_create_shared;
// if an input mapper disagrees, then weird stuff may happen.
unsigned int inputraw_translate_key(unsigned int keycode);



struct inputmouse;
struct inputjoy;



struct inputmapper {
	//Asks whether a button is pressed. If oneshot is set, the button is considered released after
	// being held for one frame. If the button is not mapped to anything, it's considered unheld.
	//It is safe to query the same button for both oneshot and non-oneshot.
	bool (*button)(struct inputmapper * this, unsigned int id, bool oneshot);
	
	//Tells which key was pressed (actually released) last. You're responsible for setting it free()
	// when you're done. Includes all relevant shift states.
	//It is undefined whether you'll miss anything if you call poll() twice without calling this. It
	// is undefined what you'll get if you call it twice without a poll() in between. It is undefined
	// which of them is returned if two keys are released at the same time.
	char * (*last)(struct inputmapper * this);
	
	//Sets which inputraw to use. The inputmapper takes ownership of this item; it can not be used
	// anymore after calling this, not even to free it (the inputmapper takes care of that).
	//Note that calling this will reset all mapped keys; an inputmapper may choose different internal
	// representations for its data depending on which inputraw is installed.
	void (*set_input)(struct inputmapper * this, struct inputraw * in);
	
	//Maps an input descriptor (a string) to an input ID.
	//IDs should be assigned to low numbers (0 is fine); leaving big unused holes is wasteful, though
	// small ones don't matter too much.
	//Input descriptors are human readable and human editable text strings, for example KB1::ShiftR+F1,
	// if the input driver maps them to RetroKeyboard properly; if the driver behaves poorly, it ends
	// up as a mess (for example KB1::x4B+x54).
	//However, no matter how they look, they should not be parsed; they can be shown to the user,
	// stored in a text file, or whatever, but don't chop them apart.
	//Note that while A+B+C is the same as B+A+C, A+C+B is not the same. The last listed key is the
	// actual trigger; the others are treated as 'shift states'. If something is mapped to A+B+C, and
	// the keys are pressed in order A C B, A+B+C will be recorded as held, but it will never actually
	// fire if queried as oneshot.
	//Also note that mapping one input can affect the others; if H is mapped somewhere, L+H will
	// trigger it, but if L+H is mapped to something else, the plain H is no longer triggered by L+H.
	//Since all keys can be shift states in that way, Ctrl, Shift and Alt are not given special treatment.
	//Non-keyboard inputs are not supported right now.
	//The return value is whether it worked; if it didn't, the input slot will be unmapped.
	//A NULL or an empty string will return success and unmap the key.
	bool (*map_key)(struct inputmapper * this, const char * descriptor, unsigned int id);
	
	//Tells the structure to ask the inputraw for updates. Clears all prior oneshot flags.
	void (*poll)(struct inputmapper * this);
	
	//Clears all input held flags, as well as last().
	void (*clear)(struct inputmapper * this);
	
	void (*free)(struct inputmapper * this);
};
struct inputmapper * inputmapper_create(struct inputraw * in);

//Takes an input descriptor and verifies that it's understandable; if it is, it's normalized into
// the mapper's favourite format, otherwise you get a NULL.
char * inputmapper_normalize(const char * descriptor);



typedef void(*funcptr)();
struct dylib {
	bool (*owned)(struct dylib * this);
	
	void* (*sym_ptr)(struct dylib * this, const char * name);
	funcptr (*sym_func)(struct dylib * this, const char * name);
	
	void (*free)(struct dylib * this);
};
struct dylib * dylib_create(const char * filename);

//Returns ".dll", ".so", ".dylib", or whatever is standard on this OS. The return value is lowercase.
const char * dylib_ext();



//Any data associated with this thread is freed once the thread procedure returns, with the possible exception of the userdata.
//It is safe to malloc() something in one thread and free() it in another.
//It is not safe to call window_run_*() from within another thread than the one entering main().
void thread_create(void(*startpos)(void* userdata), void* userdata);

//This is a simple tool that ensures only one thread is doing a certain action at a given moment.
//It may be used from multiple threads simultaneously.
//Memory barrier are inserted as appropriate. Any memory access done while holding a lock is finished while holding this lock.
//This means that if all access to an object is done exclusively while holding the lock, no further synchronization is needed.
//It is not allowed for a thread to call lock() or try_lock() while holding the lock already. It is not allowed
// for a thread to release the lock unless it holds it. It is not allowed to free() the lock while it's held.
//However, it it allowed to hold multiple locks simultaneously.
//lock() is not guaranteed to yield the CPU if it can't grab the lock. It may be implemented as a busy loop.
//Remember to create all relevant mutexes before creating a thread.
struct mutex {
	void (*lock)(struct mutex * this);
	bool (*try_lock)(struct mutex * this);
	void (*unlock)(struct mutex * this);
	
	void (*free)(struct mutex * this);
};
struct mutex * mutex_create();

//This one lets one thread wake another.
//The conceptual difference between this and a mutex is that while a mutex is intended to protect a
// shared resource from being accessed simultaneously, an event is intended to wait until another
// thread is done with something.
//An example would be a producer-consumer scenario; if one thread is producing 200 items per second,
// and another thread processes them at 100 items per second, then there will soon be a lot of
// waiting items. An event allows the consumer to ask the producer to get to work, so it'll spend
// half of its time sleeping, instead of filling the system memory.
//If the consumer is the faster one, the excess signals will be ignored. If this is an issue, you
// can use another event, alternatively just drop the excess items. (Remember to use a mutex!)
//Only one thread is allowed to be in wait() at any moment; it is undefined what happens if two
// threads wait() at once on the same event.
struct event {
	void (*signal)(struct event * this);
	void (*wait)(struct event * this);
	
	void (*free)(struct event * this);
};
struct event * event_create();



#ifdef NEED_ICON_PNG
extern const unsigned char icon_minir_64x64_png[1300];
#endif



//A compressing, lossy stack. Optimized for large, mostly similar, blocks of data; optimized for
// writing, less so for reading.
//"Lossy" means that it will discard old data if its capacity is exhausted, not that it will give
// out any memory block it wasn't given.
struct rewindstack {
	//This is equivalent to deleting and recreating the structure, with the exception that
	// it won't reallocate the big block if the capacity is unchanged. It is safe to set the capacity
	// to 0, though this will make the structure rather useless.
	//The structure may hand out bigger blocks of data than requested. This is not detectable; just
	// ignore the extra bytes.
	//The structure may allocate a reasonable multiple of blocksize, in addition to capacity.
	//It is not possible to accurately predict how many blocks will fit in the structure; it varies
	// depending on how much the data changes. Emulator savestates are usually compressed to about
	// 0.5-2% of their original size. If you're curious, you can stick in some data and use capacity().
	void (*reset)(struct rewindstack * this, size_t blocksize, size_t capacity);
	
	//Asks where to put a new block. Size is same as blocksize. Don't read from it; contents are undefined.
	//push_end or push_cancel must be the first function called on the structure after this; not even free() is allowed.
	//This function cannot fail, though a pull() directly afterwards may fail.
	void * (*push_begin)(struct rewindstack * this);
	//Tells that the savestate has been written. Don't use the pointer from push_begin after this point.
	void (*push_end)(struct rewindstack * this);
	//Tells that nothing good was written to the pointer from push_begin. Equivalent to push_end+pull,
	// but faster, and may avoid discarding something. It is allowed to have written to the pointer.
	void (*push_cancel)(struct rewindstack * this);
	
	//Pulls off a block. Don't change it; it'll be used to generate the next one. The returned pointer is only
	// guaranteed valid until the first call to any function in this structure, with the exception that capacity()
	// will not invalidate anything. If the requested block has been discarded, or was never pushed, it returns NULL.
	const void * (*pull)(struct rewindstack * this);
	
	//Tells how many entries are in the structure, how many bytes are used, and whether the structure
	// is likely to discard something if a new item is appended. The full flag is guaranteed true if
	// it has discarded anything since the last pull() or reset(); however, it may be set even before
	// discarding, if the implementation believes that will simplify the implementation.
	void (*capacity)(struct rewindstack * this, unsigned int * entries, size_t * bytes, bool * full);
	
	void (*free)(struct rewindstack * this);
};
struct rewindstack * rewindstack_create(size_t blocksize, size_t capacity);




struct libretro_core_option {
	const char * name_internal;
	const char * name_display;
	
	//This one is hackishly calculated by checking whether it's checked during retro_run.
	bool reset_only;
	
	unsigned int numvalues;
	const char * const * values;
};

enum libretro_memtype { // These IDs are the same as RETRO_MEMORY_*.
	libretromem_sram,
	libretromem_unused1,
	libretromem_wram,
	libretromem_vram
};

//This flag means the core will never change it once retro_load_rom has returned; however, the core
// will read from this area, and the frontend may assume it has permission to write to this area.
#define LIBRETRO_MEMFLAG_CONST     (1 << 0)
//This flag means that this memory area contains big endian data. Default is little endian.
#define LIBRETRO_MEMFLAG_BIGENDIAN (1 << 1)
//If this is set, all memory access in this area is aligned to their own size, and no non-power-of-two sized data exists.
//For example, no two-byte data starts at odd addresses.
#define LIBRETRO_MEMFLAG_ALIGNED   (1 << 2)

//If the core does not describe its memory maps, the frontend may create one from various RETRO_MEMORY_* as a fallback.
//Implementing this without also exposing (at least) RETRO_MEMORY_SYSTEM_RAM is highly discouraged.
//Guaranteed to not change during retro_load_game.
//The first descriptor to claim a byte in an address space is the one that applies.
//The first claimed byte in any address space to point to a physical byte is the primary way to access that byte.
struct libretro_memory_descriptor {
	uint64_t flags;
	
	//It is not allowed to do any math on the pointer; the frontend may compare them for equality.
	//If 'start' does not point to the first byte in the pointer, put the difference in 'offset'.
	void * ptr;
	size_t offset;
	
	//This is the location in the emulated address space where the mapping starts.
	size_t start;
	
	//If (this->start xor current address) & select equals zero, this mapping applies to this address.
	//Can be zero, in which case the frontend will calculate this based on 'len'.
	size_t select;
	
	//If this is nonzero, the set bits are shifted off. Applies after 'select' is used to zero unrelated bits.
	size_t disconnect;
	
	//This one tells the size of the current memory area.
	//If, after select+disconnect are applied, the address is higher than this, the highest bit of the address is cleared.
	//If the address is still too high, the next highest bit is cleared.
	//Can be zero, in which case select+1 is used. However, both select and len can't be zero simultaneously; that wouldn't make sense.
	size_t len;
	
	//To go from emulated address to physical address, the following order applies:
	//Subtract 'start', pick off 'disconnect', apply 'len', add 'offset'.
	
	//The address space name must consist of only a-zA-Z0-9_, should be as short as feasible (maximum length is 8 excluding the NUL),
	// and may not be any other address space plus one or more 0-9A-F at the end.
	//However, multiple memory descriptors for the same address space is allowed, and the address
	// space name can be empty. NULL is treated as empty.
	//Address space names are case sensitive, but avoid lowercase if possible.
	//No two mappings may have the same memory_ptr but different address space names.
	//Examples:
	// blank+blank - valid (multiple things may be mapped in the same namespace)
	// 'Sp'+'Sp' - valid (multiple things may be mapped in the same namespace)
	// 'A'+'B' - valid (neither is a prefix of each other)
	// 'S'+blank - valid ('S' is not in 0-9A-F)
	// 'a'+blank - valid ('a' is not in 0-9A-F)
	// 'a'+'A' - valid (neither is a prefix of each other)
	// 'AR'+blank - valid (the R makes it impossible for the A to be part of the address)
	// 'ARB'+blank - valid (the B can't be part of the address either, because there is no namespace 'AR')
	// blank+'B' - not valid, because it's ambigous which address space B1234 would refer to.
	//  The length can't be used for that purpose, because cheat codes have no separator between address and data,
	//   so the cheat code B123456 could be either 'set B12 to 3456' or 'set B1234 to 56'.
	//  While the lowest bit of the length could be used, it would become rather messy rather quickly.
	const char * addrspace;
	//This one is how many digits an address has in this address space. This must be the same for each mapping
	// in this address space, and the sum of this and the address space name length must be 16 or lower.
	//Can be 0, in which case the length is calculated from 'select'.
	//Should only be set in very rare cases.
	unsigned addr_str_len;
};
//Sample descriptors (minus .ptr, and LIBRETRO_MEMFLAG_ on the flags):
//SNES WRAM:
// .start=0x7E0000, .len=0x20000
//(Note that this must be mapped before the ROM in most cases; some of the ROM mappers want to claim $7E0000 for themselves.)
//SNES SPC700 RAM:
// .addrspace="S", .len=0x10000
//SNES WRAM mirrors:
// .flags=MIRROR, .start=0x000000, .select=0xC0E000, .len=0x2000
// .flags=MIRROR, .start=0x800000, .select=0xC0E000, .len=0x2000
//SNES WRAM mirrors, alternate equivalent descriptor:
// .flags=MIRROR, .select=0x40E000, .disconnect=~0x1FFF
//(Various similar constructions can be created by combining parts of the above two.)
//SNES LoROM (512KB, mirrored a couple of times):
// .flags=CONST, .start=0x008000, .select=0x408000, .disconnect=0x8000, .len=512*1024
// .flags=CONST, .start=0x400000, .select=0x400000, .disconnect=0x8000, .len=512*1024
//SNES HiROM (4MB):
// .flags=CONST,                 .start=0x400000, .select=0x400000, .len=4*1024*1024
// .flags=CONST, .offset=0x8000, .start=0x008000, .select=0x408000, .len=4*1024*1024
//SNES ExHiROM (8MB):
// .flags=CONST, .offset=4*1024*1024,        .start=0x400000, .select=0xC00000, .len=4*1024*1024
// .flags=CONST, .offset=0,                  .start=0xC00000, .select=0xC00000, .len=4*1024*1024
// .flags=CONST, .offset=4*1024*1024+0x8000, .start=0x008000, .select=0xC08000, .len=4*1024*1024
// .flags=CONST, .offset=0x8000,             .start=0x808000, .select=0xC08000, .len=4*1024*1024
//.len can be implied by .select in many of them, but was included for clarity.
//In case of inconsistency between this and the descriptions, the examples shall prevail. Then report the bug.

struct libretro {
	//Any returned pointer is, unless otherwise specified, valid only until the next call to a function here, and freed by this object.
	//Input pointers are, unless otherwise specified, not expected valid after the function returns.
	
	const char * (*name)(struct libretro * this);
	const char * const * (*supported_extensions)(struct libretro * this);//Return value format is { "smc", "sfc", NULL }.
	
	//This one is also without the dot.
	bool (*supports_extension)(struct libretro * this, const char * extension);
	
	//Whether the core supports load_rom(NULL).
	bool (*supports_no_game)(struct libretro * this);
	
	//The interface pointers must be valid during every call to run().
	//It is safe to attach new interfaces without recreating the structure.
	//It is safe to attach new interfaces if the previous ones are destroyed.
	void (*attach_interfaces)(struct libretro * this, struct video * v, struct audio * a, struct libretroinput * i);
	
	bool (*load_rom)(struct libretro * this, const char * filename);
	bool (*load_rom_mem)(struct libretro * this, const char * data, size_t datalen);
	bool (*load_rom_mem_supported)(struct libretro * this);
	
	//The following are only valid after a game is loaded.
	
	void (*get_video_settings)(struct libretro * this, unsigned int * width, unsigned int * height, unsigned int * depth, double * fps);
	double (*get_sample_rate)(struct libretro * this);
	
	//The core options will be reported as having changed on a freshly created core,
	// even if there are no options. The flag is cleared by calling this function.
	bool (*get_core_options_changed)(struct libretro * this);
	//The list is terminated by a { NULL, NULL, 0, NULL }.
	//The return value is invalidated by run() or free(), whichever comes first.
	const struct libretro_core_option * (*get_core_options)(struct libretro * this, unsigned int * numopts);
	//It is undefined behaviour to set a nonexistent option, or to set an option to a nonexistent value.
	void (*set_core_option)(struct libretro * this, unsigned int option, unsigned int value);
	unsigned int (*get_core_option)(struct libretro * this, unsigned int option);
	
	//You can write to the returned pointer.
	//Will return 0:NULL if the core doesn't know what the given memory type is.
	//(If that happens, you can still read and write the indicated amount to the pointer.)
	void (*get_memory)(struct libretro * this, enum libretro_memtype which, size_t * size, void* * ptr);
	
	const struct libretro_memory_descriptor * (*get_memory_info)(struct libretro * this, unsigned int * nummemdesc);
	
	void (*reset)(struct libretro * this);
	
	size_t (*state_size)(struct libretro * this);
	bool (*state_save)(struct libretro * this, void* state, size_t size);
	bool (*state_load)(struct libretro * this, const void* state, size_t size);
	
	void (*run)(struct libretro * this);
	
	void (*free)(struct libretro * this);
};

//The message notification may be called before libretro_create returns. It may even be called if the
// function returns NULL afterwards. It can be NULL, in which case the messages will be discarded.
//It is safe to free this item without loading a ROM.
//Since a Libretro core is a singleton, only one libretro structure may exist for each core. For the purpose of the
// previous sentence, loading the dylib through other ways than this function counts as creating a libretro structure.
//If one existed already, 'existed' will be set to true. For success, and for other failures, it's false.
struct libretro * libretro_create(const char * corepath, void (*message_cb)(int severity, const char * message), bool * existed);

//Returns whatever Libretro cores the system can find. The following locations are to be searched for all dylibs:
//- The directory of the executable
//- All subdirectories of the directory the executable is in (but not subdirectories of those)
//- Any other directory the system feels like including, including system directories
//If the scanned directories can be expected to contain large amounts of non-Libretro dylibs, all
// dylibs whose name does not contain "retro" or "core" should be filtered off. For example,
// returning the entire /usr/lib is not appropriate.
//The return value may contain duplicates, may contain non-Libretro dylibs, and may even contain non-dylibs.
//The return value is invalidated by the next call to this function, or libretro_nearby_cores.
//
//For example, the following directory structure would work out of the box:
// emulators/
//   minir.exe
//   cores/
//     snes9x_libretro.dll
//     gambatte_libretro.dll
//   roms/
//     zeldaseasons.gbc
//     kirby3.smc
const char * const * libretro_default_cores();

//Equivalent to libretro_default_cores, but looks beside the given ROM instead.
const char * const * libretro_nearby_cores(const char * rompath);



//An input mapper that converts the interface of an inputmapper to whatever a Libretro core
// understands. It's roughly a joypad emulator. The input mapper is assumed polled elsewhere.
struct libretroinput {
	//Polls input. Same interface as Libretro.
	int16_t (*query)(struct libretroinput * this, unsigned port, unsigned device, unsigned index, unsigned id);
	
	//Sets the input handler. It is still usable for other things while attached to a libretroinput.
	// It is not deleted once this structure is deleted.
	void (*set_input)(struct libretroinput * this, struct inputmapper * in);
	
	//Tells where the libretroinput should ask the inputmapper for the first used key. Order is rather
	// illogical; B, Y, Select, Start, Up, Down, Left, Right, A, X, L, R, L2, R2, L3, R3.
	//len is how many input slots should be used, including the first one. They must be consecutive,
	// and should be a multiple of 16.
	void (*joypad_set_inputs)(struct libretroinput * this, unsigned port, unsigned int inputstart, unsigned int len);
	
	//Whether to blocks left+right and up+down. Defaults to allowed.
	void (*joypad_set_block_opposing)(struct libretroinput * this, bool block);
	
	void (*free)(struct libretroinput * this);
};

struct libretroinput * libretroinput_create(struct inputmapper * in);



struct minircorelist {
	const char * path;
	const char * name;
};

struct minirconfig {
	//Only config.c may use the items starting with underscores.
	char * _name;
	char * _path;
	
	UNION_BEGIN
	//global
	STRUCT_BEGIN
		char * corename;
		char * gamename;
	STRUCT_END
	
	//cores
	STRUCT_BEGIN
		char* * _support;
		char* * _primary;
		unsigned int _support_count;
		unsigned int _primary_count;
	STRUCT_END
	
	//games
	STRUCT_BEGIN
		char * _forcecore;
	STRUCT_END
	UNION_END
	
#define CONFIG_HEADER
#include "obj/config.c"
#undef CONFIG_HEADER
	
	//this is at the end for packing reasons
	bool _autoload;
};

enum input {
#define CONFIG_HEADER_ENUM
#include "obj/config.c"
#undef CONFIG_HEADER_ENUM
input_count
};

//See minir.cfg for valid values.
extern struct minirconfig config;

//Reads the config from disk.
void config_read(const char * path);

//Tells which game to autoload. Can be NULL if none. Don't free it.
const char * config_get_autoload();

//Returns { "smc", "sfc", NULL } for all possible values. Free it when you're done, but don't free the pointers inside.
const char * * config_get_supported_extensions();

//Tells which cores support this game. Send it to free() when you're done; however, the pointers
// inside should not be freed. If a game specifies it should be opened with one specific core, only that is returned.
//count can be NULL if you don't care. Alternatively, it's terminated with a { NULL, NULL }.
struct minircorelist * config_get_core_for(const char * gamepath, unsigned int * count);

//Populates struct config with the core- or game-specific options for this. Will memorize all changed config.
//NULL is valid for either or both of them. It is not an error if a given entry doesn't exist.
//If the given game demands a specific core, the given core will be ignored. The game will always be honored unless it's NULL.
void config_load(const char * corepath, const char * gamepath);

bool config_core_supports(const char * core, const char * extension);
void config_create_core(const char * core, bool override_existing, const char * name, const char * const * supported_extensions);
void config_create_game(const char * game, bool override_existing, const char * name);
void config_set_primary_core(const char * core, const char * extension);
void config_delete_core(const char * core);
void config_delete_game(const char * game);

//Writes the changes back to the file, if there are any changes.
void config_write(const char * path);



//All functions on this object yield undefined behaviour if datsize is not within 1..4.
//This object has a fair amount of do-it-yourself mentality.
enum cheat_compfunc { cht_lt, cht_gt, cht_lte, cht_gte, cht_eq, cht_neq };
enum cheat_chngtype { cht_once, cht_inconly, cht_deconly, cht_const };
struct cheat {
	bool enabled;
	bool issigned;
	unsigned char changetype;//this is an enum cheat_chngtype, but they are hard to set the size of.
	unsigned char datsize;
	uint32_t val;
	const char * desc;
	char * addr;//For cheat_set, this is only read, and may be a casted const char *.
	            //For cheat_get, this is a caller-allocated buffer of size 32 or larger, and will be written.
};
struct minircheats_model {
	void (*set_memory)(struct minircheats_model * this, const struct libretro_memory_descriptor * memory, unsigned int nummemory);
	
	//The relevant size is how much memory it would take to create the 'prev' arrays.
	//it doesn't change depending on whether they exist, and doesn't account for malloc overhead.
	//Defaults to disabled, and switches to that on every set_memory.
	size_t (*prev_get_size)(struct minircheats_model * this);
	void (*prev_set_enabled)(struct minircheats_model * this, bool enable);
	bool (*prev_get_enabled)(struct minircheats_model * this);
	
	void (*search_reset)(struct minircheats_model * this);
	void (*search_set_datsize)(struct minircheats_model * this, unsigned int datsize);//Default 1.
	void (*search_set_signed)(struct minircheats_model * this, bool issigned);//Default unsigned.
	void (*search_do_search)(struct minircheats_model * this,
	                         enum cheat_compfunc compfunc, bool comptoprev, unsigned int compto);
	
	unsigned int (*search_get_num_rows)(struct minircheats_model * this);
	//Returns all information about a currently visible row. The data size is set by the functions above.
	//The address will be written to, and must be at least 32 bytes long (31 plus NUL).
	//If 'prev' is disabled, 'prevval' will remain unchanged if queried.
	void (*search_get_vis_row)(struct minircheats_model * this, unsigned int row,
	                           char * addr, uint32_t * val, uint32_t * prevval);
	
	//Cheat code structure:
	//disable address value signspec direction SP desc
	//disable is '-' if the cheat is currently disabled, otherwise blank.
	//address is a namespace identifier and an address inside this prefix, in hex. There is no
	// separator from the value; all addresses in a namespace have the same length.
	//value is what to set it to, also in hex. It's either two, four, six or eight digits.
	//signspec is 'S' if the cheat code is signed, or empty otherwise. For addresses
	// not allowed to change, the sign makes no difference, and should be empty.
	//direction is '+' if the address is allowed to increase, '-' if it's allowed to decrease, '.' for
	// single-use cheats, or empty if the given value should always be there.
	//SP is a simple space character. Optional if the description is blank.
	//desc is a human readable description of the cheat code. May not contain ASCII control characters
	// (0..31 and 127), but is otherwise freeform.
	//The format is designed so that a SNES Gameshark code is valid.
	
	//Fails if:
	//- There is no such namespace
	//- That address is not mapped in that namespace
	//- The relevant memory block ends too soon
	//- Alignment says you can't use that address
	//In all cases is it fair to blame the address.
	bool (*cheat_read)(struct minircheats_model * this, const char * addr, unsigned int datsize, uint32_t * val);
	
	//Don't cache cheat IDs; use the address as unique key.
	//If the address is invalid, or not targeted by any cheats, it returns -1.
	int (*cheat_find_for_addr)(struct minircheats_model * this, unsigned int datsize, const char * addr);
	
	unsigned int (*cheat_get_count)(struct minircheats_model * this);
	//To add a new cheat, use pos==count. To check if a cheat is valid without actually adding it, use pos==-1.
	//The possible errors are the same as cheat_read.
	//It is not guaranteed that cheat_get returns the same values as given to cheat_set; for example, mirroring may be undone.
	//However, it is guaranteed that setting a cheat to itself will do nothing.
	bool (*cheat_set)(struct minircheats_model * this, int pos, const struct cheat * newcheat);
	bool (*cheat_set_as_code)(struct minircheats_model * this, int pos, const char * code);
	void (*cheat_get)(struct minircheats_model * this, unsigned int pos, struct cheat * newcheat);
	const char * (*cheat_get_as_code)(struct minircheats_model * this, unsigned int pos);
	void (*cheat_set_enabled)(struct minircheats_model * this, unsigned int pos, bool enable);
	void (*cheat_remove)(struct minircheats_model * this, unsigned int pos);
	
	//This one makes all cheat codes take effect. Call it each frame.
	bool (*cheat_apply)(struct minircheats_model * this);
	
	void (*free)(struct minircheats_model * this);
};
struct minircheats_model * minircheats_create_model();

struct minircheats {
	void (*set_parent)(struct minircheats * this, struct window * parent);
	void (*set_core)(struct minircheats * this, struct libretro * core, size_t prev_limit);
	
	void (*show_search)(struct minircheats * this);
	void (*show_list)(struct minircheats * this);
	
	//This one disables all cheat codes. Does, however, not disable RAM watch.
	void (*set_enabled)(struct minircheats * this, bool enable);
	bool (*get_enabled)(struct minircheats * this);
	
	//This one should be called every frame, as soon as possible after libretro->run(). This applies even if disabled.
	//ramwatch tells whether RAM watch should be updated (both cheat search and ram watch).
	void (*update)(struct minircheats * this, bool ramwatch);
	
	unsigned int (*get_cheat_count)(struct minircheats * this);
	//The returned pointer is valid until the next get_cheat() or free().
	const char * (*get_cheat)(struct minircheats * this, unsigned int id);
	void (*set_cheat)(struct minircheats * this, unsigned int id, const char * code);
	
	void (*free)(struct minircheats * this);
};
struct minircheats * minircheats_create();



#include "image.h"





struct inputraw {
	//If the answer is zero, the implementation cannot differ between different keyboards, and will
	// return which keys are pressed on any of them.
	//The return value is guaranteed to be no higher than 32; if it would be, the excess keyboards are ignored.
	//It is guaranteed that the answer will never switch between zero and nonzero. If the implementation can differ
	// between keyboards, and none are attached, it should lie and say 1; it's allowed for polling on this keyboard to fail.
	//If a keyboard is attached or detached, it is implementation defined whether this is noticed
	// instantly, or if it remains the same until next time keyboard 0 is polled.
	unsigned int (*keyboard_num_keyboards)(struct inputraw * this);
	
	//Returns how long array keyboard_poll expects. It is same for all keyboards. It is not required
	// that all keys are possible, though too big overestimations will waste resources.
	//Key 0 is guaranteed to never fire; if it could, the implementation should poll to keys+1, and set keys[0] to zero.
	//The return value is guaranteed to be no higher than 1024; if it would be, the implementatio
	// should use a better way to map native keycodes to something below 1024. If this is not
	// possible, the excess keys are ignored.
	unsigned int (*keyboard_num_keys)(struct inputraw * this);
	
	//Returns which keyboard keys are pressed on the given keyboard. keys[] must be keyboard_num_keys()
	// bytes long.
	//If a key is not pressed, the return value in this array is 0. If it is pressed, it's something
	// else; it's implementation defined what, and not guaranteed to be the same between different
	// keys or different pollings. If polling fails for any reason, the user should pretend all keys
	// are released.
	//It is allowed for an implementation to cache the answers and only update when being polled for kb_id=0.
	bool (*keyboard_poll)(struct inputraw * this, unsigned int kb_id, unsigned char * keys);
	
	//Returns which entries in the keyboard map maps to a retro_key. The lengths of these arrays are
	// keyboard_num_keys() and RETROK_LAST, respectively. They are valid for, at least, the lifetime
	// of the structure, but may last longer. It is allowed for them to be shared between multiple
	// inputraw structures, including inputraw structures of different kinds.
	void (*keyboard_get_map)(struct inputraw * this, const unsigned int ** keycode_to_libretro,
	                                                 const unsigned int ** libretro_to_keycode);
	
	void (*free)(struct inputraw * this);
};

const char * const * inputraw_supported_backends();
struct inputraw * inputraw_create(const char * backend, uintptr_t windowhandle);



//Linux drivers:
#ifdef INPUT_X11
struct inputraw * _inputraw_create_x11(uintptr_t windowhandle);
#endif
#ifdef INPUT_X11_XINPUT2
struct inputraw * _inputraw_create_x11_xinput2(uintptr_t windowhandle);
#endif
#ifdef INPUT_GDK
struct inputraw * _inputraw_create_gdk(uintptr_t windowhandle);
#endif
#ifdef WNDPROT_X11
//All input drivers for X11 use the same keycode translation table. This one sets up
// keyboard_num_keyboards, keyboard_num_keys, and keyboard_get_map.
//keyboard_num_keys will return 256. The driver should not override this, and may hardcode this 256 anywhere it wants.
//keyboard_num_keyboards will return 0. The driver should replace this if the input driver supports separating the keyboards.
//No deallocation needed.
void _inputraw_x11_keyboard_create_shared(struct inputraw * this);
#endif

//Windows drivers:
#ifdef INPUT_RAWINPUT
struct inputraw * _inputraw_create_rawinput(uintptr_t windowhandle);
#endif
#ifdef INPUT_DIRECTINPUT
struct inputraw * _inputraw_create_directinput(uintptr_t windowhandle);
#endif
#ifdef WNDPROT_WINDOWS
//Windows drivers share keycode translations, too. (But they're obviously not the same as X11 key mappings.) This one acts the same way, including the 256.
void _inputraw_windows_keyboard_create_shared(struct inputraw * this);
#endif

struct inputraw * _inputraw_create_none(uintptr_t windowhandle);

//This one translates a hardware keycode to a Libretro code. It uses the same tables as inputraw_*_keyboard_create_shared;
// if an input mapper disagrees, then weird stuff may happen.
unsigned int _inputraw_translate_key(unsigned int keycode);
