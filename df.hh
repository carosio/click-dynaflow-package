#ifndef DF_HH
#define DF_HH

// bytes 16-32

#define GROUP_SRC_ANNO_OFFSET           16
#define GROUP_SRC_ANNO_SIZE             4
#define GROUP_SRC_ANNO(p)               ((p)->anno_u32(GROUP_SRC_ANNO_OFFSET))
#define SET_GROUP_SRC_ANNO(p, v)        ((p)->set_anno_u32(GROUP_SRC_ANNO_OFFSET, (v)))

#define GROUP_DST_ANNO_OFFSET           20
#define GROUP_DST_ANNO_SIZE             4
#define GROUP_DST_ANNO(p)               ((p)->anno_u32(GROUP_DST_ANNO_OFFSET))
#define SET_GROUP_DST_ANNO(p, v)        ((p)->set_anno_u32(GROUP_DST_ANNO_OFFSET, (v)))

#define GROUP_ANNO_SIZE                 4
#define GROUP_ANNO(p, offset)           ((p)->anno_u32((offset)))
#define SET_GROUP_ANNO(p, offset, v)    ((p)->set_anno_u32((offset), (v)))

#define CLIENT_ANNO_OFFSET              24
#define CLIENT_ANNO_SIZE                4
#define CLIENT_ANNO(p)                  ((p)->anno_u32(CLIENT_ANNO_OFFSET))
#define SET_CLIENT_ANNO(p, v)           ((p)->set_anno_u32(CLIENT_ANNO_OFFSET, (v)))

#define FLOW_ANNO_OFFSET                28
#define FLOW_ANNO_SIZE                  4
#define FLOW_ANNO(p)                    ((p)->anno_u32(FLOW_ANNO_OFFSET))
#define SET_FLOW_ANNO(p, v)             ((p)->set_anno_u32(FLOW_ANNO_OFFSET, (v)))

#define ACTION_ANNO_OFFSET              32
#define ACTION_ANNO_SIZE                1
#define ACTION_ANNO(p)                  ((p)->anno_u8(ACTION_ANNO_OFFSET))
#define SET_ACTION_ANNO(p, v)           ((p)->set_anno_u8(ACTION_ANNO_OFFSET, (v)))

enum DF_RuleAction {
    DF_RULE_UNKNOWN, // default for new Flows
    DF_RULE_NO_ACTION,
    DF_RULE_ACCEPT,
    DF_RULE_DENY,
    DF_RULE_DROP,
    _DF_RULE_SIZE,
    DF_RULE_ELSE = 255 // catch all
};

#define DF_RULE_SIZE (_DF_RULE_SIZE + 1)

#endif
