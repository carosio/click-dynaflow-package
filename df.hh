#ifndef DF_HH
#define DF_HH

/* The last 64 bit of a 48 Byte annotation are used as the Flow ID. */
# define DF_FLOWID_ANNO_OFFSET    40
# define DF_FLOWID_ANNO_SIZE    8
# define DF_FLOWID_ANNO(p)    ((p)->anno_u64(DF_FLOWID_ANNO_OFFSET))
# define SET_DF_FLOWID_ANNO(p, v)   ((p)->set_anno_u64(DF_FLOWID_ANNO_OFFSET, (v)))

struct DF_State {
    // just a placeholder right now
};

#endif
