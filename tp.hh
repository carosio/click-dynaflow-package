#ifndef TP_HH
#define TP_HH

/* The last 64 bit of a 48 Byte annotation are used as the Flow ID. */
# define TP_FLOWID_ANNO_OFFSET    40
# define TP_FLOWID_ANNO_SIZE    8
# define TP_FLOWID_ANNO(p)    ((p)->anno_u64(TP_FLOWID_ANNO_OFFSET))
# define SET_TP_FLOWID_ANNO(p, v)   ((p)->set_anno_u64(TP_FLOWID_ANNO_OFFSET, (v)))

struct TP_State {
    // just a placeholder right now
};

#endif
