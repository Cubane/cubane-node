#pragma once

// this file is svn-version.h.in
// it is intended to be run through SubWCRev
// see docs at http://tortoisesvn.net/docs/release/TortoiseSVN_en/tsvn-subwcrev.html
//
//

// this is the highest commit revision of this subtree
#define		SVN_REV			68997
#define		SVN_REVSTR		"w68997\0"

#define		SVN_REVHI		((68997>>16)&0xFFFF)
#define		SVN_REVLO		(68997&0xFFFF)

// these refer to the last commit date of this subtree
#define		SVN_DATE		"2013/02/06 14:58:42"
#define		SVN_DATE_UTC	"2013/02/06 21:58:42"

// this is the compilation time
#define		SVN_NOW			"2013/03/29 00:09:19"
#define		SVN_NOW_UTC		"2013/03/29 06:09:19"

// this is the range of repository revisions in the subtree
#define		SVN_RANGE		"69733"

// uncommitted modifications? yes => M ; no => empty string
#define		SVN_MODS		""

#define		SVN_COPYRIGHT	"Copyright © 1994-2013\0"

#define		SVN_BRANCH					"10.5 Alpha"
#define 	SVN_SHORT_VERSION_STRING	"10.5 Alpha w2013-03-29-68997\0"
#define 	SVN_VERSION_STRING			"10.5 Alpha, Build w2013-03-29-68997\0"
#define		SVN_NODE_DUMP		":{\r\nBranch: '10.5 Alpha'\r\nRange: '69733'\r\nBuildLabel: 'w2013-03-29-68997'\r\nCompile: 'Mar 29 2013 00:09:19'\r\nRev: '68997'\r\nMods: 0\r\n}\r\n\0"


#define 	SVN_BUILD_LABEL		"w2013-03-29-68997\0"
#define		SVN_COMPILE			"Last Compiled on Mar 29 2013 00:09:19\0"

