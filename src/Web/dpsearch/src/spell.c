/* Copyright (C) 2003-2008 Datapark corp. All rights reserved.
   Copyright (C) 2000-2002 Lavtech.com corp. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#include "dps_common.h"
#include "dps_utils.h"
#include "dps_unicode.h"
#include "dps_unidata.h"
#include "dps_spell.h"
#include "dps_xmalloc.h"
#include "dps_word.h"
#include "dps_synonym.h"
#include "dps_hash.h"
#include "dps_vars.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef   HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif


#define MAXNORMLEN 256
#define MAX_NORM 512
#define ERRSTRSIZE 100


/*#define DEBUG_UNIREG*/

/* Unicode regex lite BEGIN */

static const dpsunicode_t *DpsUniRegTok(const dpsunicode_t *s, const dpsunicode_t **last) {
	if(s == NULL && (s=*last) == NULL)
		return NULL;

	switch(*s){
		case 0:
			return(NULL);
			break;
		case '[':
			for(*last=s+1;(**last)&&(**last!=']');(*last)++);
			if(**last==']')(*last)++;
			break;
		case '$':
		case '^':
			*last=s+1;
			break;
		default:
			for(*last=s+1;(**last)&&(**last!=']')&&(**last!='[')&&(**last!='^')&&(**last!='$');(*last)++);
			break;
	}
	return s;
}

static int DpsUniRegComp(DPS_UNIREG_EXP *reg, const dpsunicode_t *pattern) {
	const dpsunicode_t *tok, *lt;

	reg->ntokens=0;
	reg->Token=NULL;

	tok=DpsUniRegTok(pattern,&lt);
	while(tok){
		size_t len;
		reg->Token=(DPS_UNIREG_TOK*)DpsRealloc(reg->Token,sizeof(*reg->Token)*(reg->ntokens+1));
		if (reg->Token == NULL) {
		  reg->ntokens = 0;
		  return DPS_ERROR;
		}
		len=lt-tok;
		reg->Token[reg->ntokens].str = (dpsunicode_t*)DpsMalloc((len+1)*sizeof(dpsunicode_t));
		dps_memmove(reg->Token[reg->ntokens].str, tok, len * sizeof(dpsunicode_t));
		reg->Token[reg->ntokens].str[len]=0;
		
		reg->ntokens++;
		tok=DpsUniRegTok(NULL,&lt);
	}
	return DPS_OK;
}


static int DpsUniRegExec(const DPS_UNIREG_EXP *reg, const dpsunicode_t *string) {
	const dpsunicode_t *start = string;
	int match=0;
#ifdef DEBUG_UNIREG
	DPS_CHARSET *k = DpsGetCharSet("koi8-r");
	DPS_CHARSET *sy = DpsGetCharSet("sys-int");
	DPS_CONV fromuni;
	char sstr[1024];
	char rstr[1024];
	
	DpsConvInit(&fromuni, sy, k, NULL, 0);
#endif
	
	for(start=string;*start;start++){
		const dpsunicode_t *tstart=start;
		size_t i;
		
		for(i=0;i<reg->ntokens;i++){
			const dpsunicode_t *s;
			int inc=DPS_UNIREG_INC;
#ifdef DEBUG_UNIREG

			DpsConv(&fromuni, sstr, 1024, (char*)tstart, 1024);
			DpsConv(&fromuni, rstr, 1024, (char*)reg->Token[i].str, 1024);
			printf("t:%d tstart='%s'\ttok='%s'\t", i, sstr, rstr);
#endif
			switch(reg->Token[i].str[0]){
				case '^':
					if(string!=tstart){
						match=0;
					}else{	
						match=1;
					}
					break;
				case '[':
					match=0;
					for(s=reg->Token[i].str+1;*s;s++){
						if(*s==']'){
						}else
						if(*s=='^'){
							inc=DPS_UNIREG_EXC;
							match=1;
						}else{
							if((*tstart==*s)&&(inc==DPS_UNIREG_EXC)){
								match=0;
								break;
							}
							if((*tstart==*s)&&(inc==DPS_UNIREG_INC)){
								match=1;
								break;
							}
						}
					}
					tstart++;
					break;
				case '$':
					if(*tstart!=0){
						match=0;
					}else{
						match=1;
					}
					break;
				default:
					match=1;
					for(s=reg->Token[i].str;(*s)&&(*tstart);s++,tstart++){
						if(*s=='.'){
							/* Any char */
						}else
						if((*s)!=(*tstart)){
							match=0;
							break;
						}
					}
					if((*s)&&(!*tstart))match=0;
					break;
			}
#ifdef DEBUG_UNIREG
			printf("match=%d\n",match);
#endif
			if(!match)break;
		}
		if(match)break;
	}

#ifdef DEBUG_UNIREG
	printf("return match=%d\n",match);
#endif
	return match;

}


static void DpsUniRegFree(DPS_UNIREG_EXP *reg){
	size_t i;
	
	for(i=0;i<reg->ntokens;i++)
		if(reg->Token[i].str)
			DPS_FREE(reg->Token[i].str);

	DPS_FREE(reg->Token);
}


/* Unicode regex lite END */


void DpsUniRegCompileAll(DPS_ENV *Conf) {
        size_t i;
	
	for(i = 0; i < Conf->Affixes.naffixes; i++) {
	  if(!DpsUniRegComp(&Conf->Affixes.Affix[i].reg, Conf->Affixes.Affix[i].mask)) {
	    Conf->Affixes.Affix[i].compile = 0;
	  }
	}
}

static int cmpspellword(dpsunicode_t *w1, const dpsunicode_t *w2) {
    register dpsunicode_t u1 = (*w1 & 255), u2 = (*w2 & 255);
    if (u1 < u2) return -1;
    if (u1 > u2) return 1;
    if (u1 == 0) return 0;
    return DpsUniStrCmp(w1 + 1, w2 + 1);
}

static int cmpspell(const void *s1, const void *s2) {
  int lc;
  lc = strcmp(((const DPS_SPELL*)s1)->lang,((const DPS_SPELL*)s2)->lang);
  if (lc == 0) {
    lc = cmpspellword(((const DPS_SPELL*)s1)->word, ((const DPS_SPELL*)s2)->word );
  }
  return lc;
}

static int cmpaffix(const void *s1,const void *s2){
  int lc;
  if (((const DPS_AFFIX*)s1)->type < ((const DPS_AFFIX*)s2)->type) {
    return -1;
  }
  if (((const DPS_AFFIX*)s1)->type > ((const DPS_AFFIX*)s2)->type) {
    return 1;
  }
  lc = strcmp(((const DPS_AFFIX*)s1)->lang,((const DPS_AFFIX*)s2)->lang);
  if (lc == 0) {
    if ( (((const DPS_AFFIX*)s1)->replen == 0) && (((const DPS_AFFIX*)s2)->replen == 0) ) {
      return 0;
    }
    if (((const DPS_AFFIX*)s1)->replen == 0) {
      return -1;
    }
    if (((const DPS_AFFIX*)s2)->replen == 0) {
      return 1;
    }
    {
      dpsunicode_t u1[BUFSIZ], u2[BUFSIZ];
      DpsUniStrCpy(u1,((const DPS_AFFIX*)s1)->repl); 
      DpsUniStrCpy(u2,((const DPS_AFFIX*)s2)->repl); 
      if (((const DPS_AFFIX*)s1)->type == 'p') {
	*u1 &= 255; *u2 &= 255;
	return DpsUniStrCmp(u1, u2);
      } else {
	u1[((const DPS_AFFIX*)s1)->replen - 1] &= 255; u2[((const DPS_AFFIX*)s2)->replen -1] &= 255;
	return DpsUniStrBCmp(u1, u2);
      }
    }
  }
  return lc;
}

int DpsSpellAdd(DPS_SPELLLIST *List, const dpsunicode_t *word, const char *flag, const char *lang) {
	if(List->nspell>=List->mspell){
		List->mspell += 1024; /* was: 1024 * 20 */
		List->Spell=(DPS_SPELL *)DpsXrealloc(List->Spell,List->mspell*sizeof(DPS_SPELL));
		if (List->Spell == NULL) return DPS_ERROR;
	}
	List->Spell[List->nspell].word = DpsUniRDup(word);
	dps_strncpy(List->Spell[List->nspell].flag,flag,10);
	dps_strncpy(List->Spell[List->nspell].lang,lang,5);
	List->Spell[List->nspell].lang[5] = List->Spell[List->nspell].flag[10] = '\0';
	List->nspell++;
	return DPS_OK;
}


__C_LINK int __DPSCALL DpsImportDictionary(DPS_ENV * Conf, const char *lang, const char *charset,
				   const char *filename, int skip_noflag, const char *first_letters){
        struct stat     sb;
	char *str, *data = NULL, *cur_n = NULL;	
	char *lstr;	
	dpsunicode_t *ustr;	
	DPS_CHARSET *sys_int;
	DPS_CHARSET *dict_charset;
	DPS_CONV touni;
	DPS_CONV fromuni;
	int             fd;
	char            savebyte;

	if ((lstr = (char*) DpsMalloc(2048)) == NULL) {
	  DPS_FREE(str);
	  return DPS_ERROR; 
	}
	if ((ustr = (dpsunicode_t*) DpsMalloc(8192)) == NULL) {
	  DPS_FREE(lstr);
	  return DPS_ERROR; 
	}

	dict_charset = DpsGetCharSet(charset);
	sys_int = DpsGetCharSet("sys-int");
	if ((dict_charset == NULL) || (sys_int == NULL)) {
	  DPS_FREE(lstr);
	  DPS_FREE(ustr);
	  return DPS_ERROR;
	}
	
	DpsConvInit(&touni, dict_charset, sys_int, Conf->CharsToEscape, 0);
	DpsConvInit(&fromuni, sys_int, dict_charset, Conf->CharsToEscape, 0);
	
	if (stat(filename, &sb)) {
	  fprintf(stderr, "Unable to stat synonyms file '%s': %s", filename, strerror(errno));
	  DPS_FREE(lstr);
	  DPS_FREE(ustr);
	  return DPS_ERROR;
	}
	if ((fd = DpsOpen2(filename, O_RDONLY)) <= 0) {
	  fprintf(stderr, "Unable to open synonyms file '%s': %s", filename, strerror(errno));
	  return DPS_ERROR;
	}
	if ((data = (char*)DpsMalloc(sb.st_size + 1)) == NULL) {
	  fprintf(stderr, "Unable to alloc %ld bytes", (long)sb.st_size);
	  DpsClose(fd);
	  DPS_FREE(lstr);
	  DPS_FREE(ustr);
	  return DPS_ERROR;
	}
	if (read(fd, data, sb.st_size) != (ssize_t)sb.st_size) {
	  fprintf(stderr, "Unable to read synonym file '%s': %s", filename, strerror(errno));
	  DPS_FREE(data);
	  DpsClose(fd);
	  DPS_FREE(lstr);
	  DPS_FREE(ustr);
	  return DPS_ERROR;
	}
	data[sb.st_size] = '\0';
	str = data;
	cur_n = strchr(str, '\n');
	if (cur_n != NULL) {
	  cur_n++;
	  savebyte = *cur_n;
	  *cur_n = '\0';
	}

	DpsClose(fd);
	while(str != NULL) {
		char *s;
		const char *flag;
		int res;
		
	        flag = NULL;
		s = str;
		while(*s){
			if(*s == '\r') *s = '\0';
			if(*s == '\n') *s = '\0';
			s++;
		}
		if((s=strchr(str,'/'))){
			*s=0;
			s++;flag=s;
			while(*s){
				if(((*s>='A')&&(*s<='Z'))||((*s>='a')&&(*s<='z')))s++;
				else{
					*s=0;
					break;
				}
			}
		}else{
			if(skip_noflag)	goto loop_continue;
			flag="";
		}

		res = DpsConv(&touni, (char*)ustr, 8192, str, 1024);
		DpsUniStrToLower(ustr);

		/* Dont load words if first letter is not required */
		/* It allows to optimize loading at  search time   */
		if(*first_letters) {
			DpsConv(&fromuni, lstr, 2048, ((const char*)ustr),(size_t)res);
			if(!strchr(first_letters,lstr[0]))
				goto loop_continue;
		}
		res = DpsSpellAdd(&Conf->Spells,ustr,flag,lang);
		if (res != DPS_OK) {
		  DPS_FREE(lstr);
		  DPS_FREE(ustr); DPS_FREE(data);
		  return res;
		}
		if (Conf->Flags.use_accentext) {
		  dpsunicode_t *af_uwrd = DpsUniAccentStrip(ustr);
		  if (DpsUniStrCmp(af_uwrd, ustr) != 0) {
		    res = DpsSpellAdd(&Conf->Spells, af_uwrd, flag, lang);
		    if (res != DPS_OK) {
		      DPS_FREE(lstr);
		      DPS_FREE(ustr); DPS_FREE(data); DPS_FREE(af_uwrd);
		      return res;
		    }
		  }
		  DPS_FREE(af_uwrd);
		  if (strncasecmp(lang, "de", 2) == 0) {
		    dpsunicode_t *de_uwrd = DpsUniGermanReplace(ustr);
		    if (DpsUniStrCmp(de_uwrd, ustr) != 0) {
		      res = DpsSpellAdd(&Conf->Spells, de_uwrd, flag, lang);
		      if (res != DPS_OK) {
			DPS_FREE(lstr);
			DPS_FREE(ustr); DPS_FREE(data); DPS_FREE(de_uwrd);
			return res;
		      }
		    }
		    DPS_FREE(de_uwrd);
		  }
		}
	loop_continue:
		str = cur_n;
		if (str != NULL) {
		  *str = savebyte;
		  cur_n = strchr(str, '\n');
		  if (cur_n != NULL) {
		    cur_n++;
		    savebyte = *cur_n;
		    *cur_n = '\0';
		  }
		}
	}
	DPS_FREE(data);
	DPS_FREE(lstr);
	DPS_FREE(ustr);
	return DPS_OK;
}




static DPS_SPELL ** DpsFindWord(DPS_AGENT * Indexer, const dpsunicode_t *p_word, const char *affixflag, DPS_PSPELL *PS, DPS_PSPELL *FZ) {
  int l,c,r,resc,resl,resr, nlang = /*Indexer->SpellLang*/ -1 /*Indexer->spellang FIXME: if Language limit issued in query */, 
    li, li_from, li_to, i, ii;
  DPS_SPELLLIST *SpellList=&Indexer->Conf->Spells;
  dpsunicode_t *word = DpsUniRDup(p_word);

  if (nlang == -1) {
    li_from = 0; li_to = SpellList->nLang;
  } else {
    li_from = nlang; li_to = li_from + 1;
  }
  if (Indexer->Conf->Spells.nspell) {
    i = (int)(*word) & 255;
    for(li = li_from; li < li_to; li++) {
      l = SpellList->SpellTree[li].Left[i];
      r = SpellList->SpellTree[li].Right[i];
      if (l == -1) continue;
      while(l<=r){
	c = (l + r) >> 1;
	resc = cmpspellword(SpellList->Spell[c].word, word);
	if( (resc == 0) && 
	    ((affixflag == NULL) || (strstr(SpellList->Spell[c].flag, affixflag) != NULL)) ) {
	  if (PS->nspell < MAX_NORM - 1) {
	    PS->cur[PS->nspell] = &SpellList->Spell[c];
	    PS->nspell++;
	    PS->cur[PS->nspell] = NULL;
	  }
	  break;
	}
/*	resl = cmpspellword(SpellList->Spell[l].word, word);
	if( (resl == 0) && 
	    ((affixflag == NULL) || (strstr(SpellList->Spell[l].flag, affixflag) != NULL)) ) {
	  if (PS->nspell < MAX_NORM - 1) {
	    PS->cur[PS->nspell] = &SpellList->Spell[l];
	    PS->nspell++;
	    PS->cur[PS->nspell] = NULL;
	  }
	  resc = resl;
	  break;
	}
	resr = cmpspellword(SpellList->Spell[r].word, word);
	if( (resr == 0) && 
	    ((affixflag == NULL) || (strstr(SpellList->Spell[r].flag, affixflag) != NULL)) ) {
	  if (PS->nspell < MAX_NORM - 1) {
	    PS->cur[PS->nspell] = &SpellList->Spell[r];
	    PS->nspell++;
	    PS->cur[PS->nspell] = NULL;
	  }
	  resc = resr;
	  break;
	}
*/
	if(resc < 0){
	  l = c + 1;
/*	  r--;*/
	} else /*if(resc > 0)*/ {
	  r = c - 1;
/*	  l++;*/
/*	} else {
	  l++;
	  r--;*/
	}
      }

      for (ii = c - 1; ii >= l; ii--) {
	resc = cmpspellword(SpellList->Spell[ii].word, word);
	if(resc == 0) {
	  if ((affixflag == NULL) || (strstr(SpellList->Spell[ii].flag, affixflag) != NULL))  {
	    if (PS->nspell < MAX_NORM - 1) {
	      PS->cur[PS->nspell] = &SpellList->Spell[ii];
	      PS->nspell++;
	      PS->cur[PS->nspell] = NULL;
	    }
	  }
	} else break;
      }
      for (ii = c + 1; ii <= r; ii++) {
	resc = cmpspellword(SpellList->Spell[ii].word, word);
	if(resc == 0) {
	  if ((affixflag == NULL) || (strstr(SpellList->Spell[ii].flag, affixflag) != NULL))  {
	    if (PS->nspell < MAX_NORM - 1) {
	      PS->cur[PS->nspell] = &SpellList->Spell[ii];
	      PS->nspell++;
	      PS->cur[PS->nspell] = NULL;
	    }
	  }
	} else break;
      }

      if ((FZ != NULL) && (PS->nspell == 0) && ((affixflag == NULL) || (strstr(SpellList->Spell[c].flag, affixflag) != NULL)) ) {
	register size_t z; /*, lw = DpUniLen(word), ls = DpsUniLen(SpellList->Spell[c].word[z]);*/
	for (z = 0; word[z] && SpellList->Spell[c].word[z] == word[z]; z++);
	if (z > 3) { /* FIXME: min. 4 letters from the end, - add config parameter for that */ 
	  if (z > FZ->nspell) {
	    DPS_FREE(FZ->cur[0]->word);
	    dps_memmove(FZ->cur[0]->flag, SpellList->Spell[c].flag, sizeof(FZ->cur[0]->flag));
	    dps_memmove(FZ->cur[0]->lang, SpellList->Spell[c].lang, sizeof(FZ->cur[0]->lang));
	    FZ->cur[0]->word = DpsUniDup(word);
	    FZ->nspell = z;
	  } else if (z == FZ->nspell) {
	    size_t flen0 = dps_strlen(FZ->cur[0]->flag);
	    dps_strncat(FZ->cur[0]->flag + flen0, SpellList->Spell[c].flag, sizeof(FZ->cur[0]->flag) - flen0);
	  }
	}
	{
	  register int q;
	  for (q = c - 1; q >= 0; q--) {
	    register size_t y;
	    for (y = 0; word[y] && SpellList->Spell[q].word[y] == word[y]; y++);
	    if (y < 3) break;
	    if (y < z) continue;
	    if (strlen(FZ->cur[0]->flag) < strlen(SpellList->Spell[q].flag)) {
	      DPS_FREE(FZ->cur[0]->word);
	      dps_memmove(FZ->cur[0]->flag, SpellList->Spell[q].flag, sizeof(FZ->cur[0]->flag));
	      dps_memmove(FZ->cur[0]->lang, SpellList->Spell[q].lang, sizeof(FZ->cur[0]->lang));
	      FZ->cur[0]->word = DpsUniDup(word);
	      FZ->nspell = z;
	    }
	  }
	  for (q = c + 1; q <= SpellList->SpellTree[li].Right[i]; q++) {
	    register size_t y;
	    for (y = 0; word[y] && SpellList->Spell[q].word[y] == word[y]; y++);
	    if (y < 3) break;
	    if (y < z) continue;
	    if (strlen(FZ->cur[0]->flag) <= strlen(SpellList->Spell[q].flag)) {
	      DPS_FREE(FZ->cur[0]->word);
	      dps_memmove(FZ->cur[0]->flag, SpellList->Spell[q].flag, sizeof(FZ->cur[0]->flag));
	      dps_memmove(FZ->cur[0]->lang, SpellList->Spell[q].lang, sizeof(FZ->cur[0]->lang));
	      FZ->cur[0]->word = DpsUniDup(word);
	      FZ->nspell = z;
	    }
	  }
	}
	
      }
      

    }
  }

  DPS_FREE(word);
  return PS->cur;
}


int DpsAffixAdd(DPS_AFFIXLIST *List, const char *flag, const char * lang, const dpsunicode_t *mask, const dpsunicode_t *find, 
		const dpsunicode_t *repl, int type) {

	if(List->naffixes>=List->maffixes){
		List->maffixes+=16;
		List->Affix = DpsXrealloc(List->Affix,List->maffixes*sizeof(DPS_AFFIX));
		if (List->Affix == NULL) return DPS_ERROR;
	}

	List->Affix[List->naffixes].compile = 1;
	List->Affix[List->naffixes].flag[0] = flag[0];
	List->Affix[List->naffixes].flag[1] = flag[1];
	List->Affix[List->naffixes].flag[2] = '\0';
	List->Affix[List->naffixes].type=type;
	dps_strncpy(List->Affix[List->naffixes].lang, lang, 5);
	List->Affix[List->naffixes].lang[5] = 0;
	
	DpsUniStrNCpy(List->Affix[List->naffixes].mask, mask, 40);
	DpsUniStrNCpy(List->Affix[List->naffixes].find,find, 15);
	DpsUniStrNCpy(List->Affix[List->naffixes].repl,repl, 15);
	
	List->Affix[List->naffixes].replen  = DpsUniLen(repl);
	List->Affix[List->naffixes].findlen = DpsUniLen(find);
	List->naffixes++;
	return DPS_OK;
}

static char * remove_spaces(char *dist,char *src){
char *d,*s;
	d=dist;
	s=src;
	while(*s){
		if((*s != ' ')&& (*s != '-') && (*s != '\t')){
			*d=*s;
			d++;
		}
		s++;
	}
	*d=0;
	return(dist);
}


__C_LINK int __DPSCALL DpsImportAffixes(DPS_ENV * Conf,const char *lang, const char*charset, 
				const char *filename) {
  struct stat     sb;
  char      *str, *data = NULL, *cur_n = NULL;
  char flag[2]="";
  char mstr[14*BUFSIZ]="";
  char mask[14*BUFSIZ]="";
  char find[14*BUFSIZ]="";
  char repl[14*BUFSIZ]="";
  char *s;
  int i;
  int suffixes=0;
  int prefixes=0;
  int IspellUsePrefixes;
  FILE *affix;
  DPS_CHARSET *affix_charset = NULL;
  dpsunicode_t unimask[BUFSIZ];
  dpsunicode_t ufind[BUFSIZ];
  dpsunicode_t urepl[BUFSIZ];
  size_t len;
  DPS_CHARSET *sys_int;
  DPS_CONV touni;
  int             fd;
  char            savebyte;
#ifdef DEBUG_UNIREG
  DPS_CONV fromuni;
#endif
#ifdef WITH_PARANOIA
  void *paran = DpsViolationEnter(paran);
#endif

  if (stat(filename, &sb)) {
    fprintf(stderr, "Unable to stat synonyms file '%s': %s", filename, strerror(errno));
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  if ((fd = DpsOpen2(filename, O_RDONLY)) <= 0) {
    dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "Unable to open synonyms file '%s': %s", filename, strerror(errno));
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  if ((data = (char*)DpsMalloc(sb.st_size + 1)) == NULL) {
    dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "Unable to alloc %d bytes", sb.st_size);
    DpsClose(fd);
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  if (read(fd, data, sb.st_size) != (ssize_t)sb.st_size) {
    dps_snprintf(Conf->errstr, sizeof(Conf->errstr)-1, "Unable to read synonym file '%s': %s", filename, strerror(errno));
    DPS_FREE(data);
    DpsClose(fd);
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  data[sb.st_size] = '\0';
  str = data;
  cur_n = strchr(str, '\n');
  if (cur_n != NULL) {
    cur_n++;
    savebyte = *cur_n;
    *cur_n = '\0';
  }
  DpsClose(fd);

  affix_charset = DpsGetCharSet(charset);
  if (affix_charset == NULL) {
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
  sys_int = DpsGetCharSet("sys-int");
  if (sys_int == NULL) {
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return 1;
  }
	    
  DpsConvInit(&touni, affix_charset, sys_int, Conf->CharsToEscape, 0);
#ifdef DEBUG_UNIREG
  DpsConvInit(&fromuni, sys_int, affix_charset, Conf->CharsToEscape, 0);
#endif

  IspellUsePrefixes = strcasecmp(DpsVarListFindStr(&Conf->Vars,"IspellUsePrefixes","no"),"no");

  while(str != NULL) {
    str = DpsTrim(str, "\t ");
    if(!strncasecmp(str,"suffixes",8)){
      suffixes=1;
      prefixes=0;
      goto loop2_continue;
    }
    if(!strncasecmp(str,"prefixes",8)){
      suffixes=0;
      prefixes=1;
      goto loop2_continue;
    }
    if(!strncasecmp(str,"flag ",5)){
      s=str+5;
      while(strchr("* ",*s))s++;
      flag[0] = s[0];
      flag[1] = (s[1] >= 'A') ? s[1] : '\0';
      goto loop2_continue;
    }
    if((!suffixes)&&(!prefixes)) goto loop2_continue;
    if((prefixes)&&(!IspellUsePrefixes)) goto loop2_continue;
		
    if((s=strchr(str,'#')))*s=0;
    if(!*str) goto loop2_continue;

    dps_strcpy(mask,"");
    dps_strcpy(find,"");
    dps_strcpy(repl,"");

    i=sscanf(str,"%[^>\n]>%[^,\n],%[^\n]",mask,find,repl);

    remove_spaces(mstr, repl); dps_strcpy(repl, mstr);
    remove_spaces(mstr, find); dps_strcpy(find, mstr);
    remove_spaces(mstr, mask); dps_strcpy(mask, mstr);

    switch(i){
    case 3:break;
    case 2:
      if(*find != '\0'){
	dps_strcpy(repl,find);
	dps_strcpy(find,"");
      }
      break;
    default:
      goto loop2_continue;
    }

    len=DpsConv(&touni,(char*)urepl,sizeof(urepl),repl, dps_strlen(repl)+1);
    DpsUniStrToLower(urepl);
#ifdef DEBUG_UNIREG
    DpsConv(&fromuni,repl,sizeof(repl),(char *)urepl,len);
#endif
		    
    len=DpsConv(&touni,(char*)ufind,sizeof(ufind),find, dps_strlen(find)+1);
    DpsUniStrToLower(ufind);
#ifdef DEBUG_UNIREG
    DpsConv(&fromuni,find,sizeof(find),(char*)ufind,len);
#endif

    if (suffixes) {
      sprintf(mstr, "%s$", mask);
    } else {
      sprintf(mstr, "^%s", mask);
    }

    len = DpsConv(&touni, (char*)unimask, sizeof(unimask), mstr, dps_strlen(mstr) + 1);
    DpsUniStrToLower(unimask);
#ifdef DEBUG_UNIREG
    DpsConv(&fromuni, mask, sizeof(mask), (char*)unimask, len);
#endif

/*    fprintf(stderr, "-- %s | %c | %s : %s > %s\n", flag, suffixes ? 's' : 'p', mask, find, repl);*/

    DpsAffixAdd(&Conf->Affixes, flag, lang, unimask, ufind, urepl, suffixes ? 's' : 'p');
    if (Conf->Flags.use_accentext) {
      dpsunicode_t *af_unimask = DpsUniAccentStrip(unimask);
      dpsunicode_t *af_ufind = DpsUniAccentStrip(ufind);
      dpsunicode_t *af_urepl = DpsUniAccentStrip(urepl);
      size_t zz;
      for (zz = 0; zz < 2; zz++) {
	if (DpsUniStrCmp(af_unimask, unimask) || DpsUniStrCmp(af_ufind, ufind) || DpsUniStrCmp(af_urepl, urepl)) {
	  size_t w_len = DpsUniLen(af_unimask);
	  dpsunicode_t *new_wrd = DpsMalloc(2 * sizeof(dpsunicode_t) * w_len);
	  if (new_wrd == NULL) {
	    DpsAffixAdd(&Conf->Affixes, flag, lang, af_unimask, af_ufind, af_urepl, suffixes ? 's' : 'p');
	  } else {
	    size_t ii, pnew = 0;
	    int in_pattern = 0;
	    for (ii = 0; ii < w_len; ii++) {
	      if ((af_unimask[ii] == (dpsunicode_t)'[') && (af_unimask[ii + 1] == (dpsunicode_t)'^') ) {
		in_pattern = 1;
	      } else if (in_pattern && (af_unimask[ii] == (dpsunicode_t)']')) {
		in_pattern = 0;
	      } else if (in_pattern && (af_unimask[ii] != unimask[ii])) {
		new_wrd[pnew++] = unimask[ii];
	      }
	      new_wrd[pnew++] = af_unimask[ii];
	    }
	    new_wrd[pnew] = 0;
	    DpsAffixAdd(&Conf->Affixes, flag, lang, new_wrd, af_ufind, af_urepl, suffixes ? 's' : 'p');
	    DPS_FREE(new_wrd);
	  }
	}
	DPS_FREE(af_unimask); DPS_FREE(af_ufind); DPS_FREE(af_urepl);
	if ((zz == 0) && (strncasecmp(lang, "de", 2) == 0)) {
	  af_unimask = DpsUniGermanReplace(unimask);
	  af_ufind = DpsUniGermanReplace(ufind);
	  af_urepl = DpsUniGermanReplace(urepl);
	} else zz = 2;
      }
    }
  loop2_continue:
    str = cur_n;
    if (str != NULL) {
      *str = savebyte;
      cur_n = strchr(str, '\n');
      if (cur_n != NULL) {
	cur_n++;
	savebyte = *cur_n;
	*cur_n = '\0';
      }
    }
  }
  DPS_FREE(data);
	    
#ifdef WITH_PARANOIA
  DpsViolationExit(-1, paran);
#endif
  return 0;
}

__C_LINK void __DPSCALL DpsSortDictionary(DPS_SPELLLIST * List){
  int  j, CurLet = -1, Let;size_t i;
  char *CurLang = NULL;

        if (List->nspell > 1) DpsSort((void*)List->Spell, List->nspell, sizeof(DPS_SPELL), cmpspell);
	for(i = 0; i < List->nspell; i++) {
	  if (CurLang == NULL || strncmp(CurLang, List->Spell[i].lang, 2) != 0) {
	    CurLang = List->Spell[i].lang;
	    dps_strncpy(List->SpellTree[List->nLang].lang, CurLang, 2);
	    List->SpellTree[List->nLang].lang[3] = 0;
	    for(j = 0; j < 256; j++)
	      List->SpellTree[List->nLang].Left[j] =
		List->SpellTree[List->nLang].Right[j] = -1;
	    if (List->nLang > 0) {
	      CurLet = -1;
	    }
	    List->nLang++;
	  }
	  Let = (int)(*(List->Spell[i].word)) & 255;
	  if (CurLet != Let) {
	    List->SpellTree[List->nLang-1].Left[Let] = i;
	    CurLet = Let;
	  }
	  List->SpellTree[List->nLang-1].Right[Let] = i;
	}
}

__C_LINK void __DPSCALL DpsSortAffixes(DPS_AFFIXLIST *List, DPS_SPELLLIST *SL) {
  int  CurLetP = -1, CurLetS = -1, Let, cl = -1;
  char *CurLangP = NULL, *CurLangS = NULL;
  DPS_AFFIX *Affix; size_t i, j;

  if (List->naffixes > 1)
    DpsSort((void*)List->Affix,List->naffixes,sizeof(DPS_AFFIX),cmpaffix);

  for(i = 0; i < SL->nLang; i++)
    for(j = 0; j < 256; j++) {
      List->PrefixTree[i].Left[j] = List->PrefixTree[i].Right[j] = -1;
      List->SuffixTree[i].Left[j] = List->SuffixTree[i].Right[j] = -1;
    }

  for(i = 0; i < List->naffixes; i++) {
    Affix = &(((DPS_AFFIX*)List->Affix)[i]);
    if(Affix->type == 'p') {
      if (CurLangP == NULL || strcmp(CurLangP, Affix->lang) != 0) {
	cl = -1;
	for (j = 0; j < SL->nLang; j++) {
	  if (strncmp(SL->SpellTree[j].lang, Affix->lang, 2) == 0) {
	    cl = j;
	    break;
	  }
	}
	CurLangP = Affix->lang;
	dps_strcpy(List->PrefixTree[cl].lang, CurLangP);
	CurLetP = -1;
      }
      if (cl < 0) continue; /* we have affixes without spell for this lang */
      Let = (int)(*(Affix->repl)) & 255;
      if (CurLetP != Let) {
	List->PrefixTree[cl].Left[Let] = i;
	CurLetP = Let;
      }
      List->PrefixTree[cl].Right[Let] = i;
    } else {
      if (CurLangS == NULL || strcmp(CurLangS, Affix->lang) != 0) {
	cl = -1;
	for (j = 0; j < SL->nLang; j++) {
	  if (strcmp(SL->SpellTree[j].lang, Affix->lang) == 0) {
	    cl = j;
	    break;
	  }
	}
	CurLangS = Affix->lang;
	dps_strcpy(List->SuffixTree[cl].lang, CurLangS);
	CurLetS = -1;
      }
      if (cl < 0) continue; /* we have affixes without spell for this lang */
      Let = (Affix->replen) ? (int)(Affix->repl[Affix->replen-1]) & 255 : 0;
      if (CurLetS != Let) {
	List->SuffixTree[cl].Left[Let] = i;
	CurLetS = Let;
      }
      List->SuffixTree[cl].Right[Let] = i;
    }
  }
}

static void CheckSuffix(const dpsunicode_t *word, size_t len, DPS_AFFIX *Affix, int *res, DPS_AGENT *Indexer, 
			DPS_PSPELL *PS, DPS_PSPELL * FZ) {
  dpsunicode_t newword[2*MAXNORMLEN] = {0};
  int err;
/*3.1  int curlang, curspellang;*/
#ifdef WITH_PARANOIA
  void *paran = DpsViolationEnter(paran);
#endif
  
  *res = DpsUniStrBNCmp(word, Affix->repl, Affix->replen);
  if (*res < 0) {
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return;
  }
  if (*res > 0) {
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return;
  }
  DpsUniStrCpy(newword, word);
  DpsUniStrCpy(newword+len-Affix->replen, Affix->find);

  if (Affix->compile) {
    err = DpsUniRegComp(&(Affix->reg), Affix->mask);
    if(err){
      DpsUniRegFree(&(Affix->reg));
#ifdef WITH_PARANOIA
      DpsViolationExit(-1, paran);
#endif
      return;
    }
    Affix->compile = 0;
  }
  if((err=DpsUniRegExec(&(Affix->reg),newword))){
    DPS_SPELL **curspell;

    if((curspell = DpsFindWord(Indexer, newword, Affix->flag, PS, FZ))) {

/*3.1 FIXME: language statistics collection while normalizing       
      curlang = Indexer->curlang;
      curspellang = Indexer->spellang;
      DpsSelectLang(Indexer, curspell->lang);
      Indexer->lang[Indexer->curlang].count++;
      Indexer->curlang = curlang;
      Indexer->spellang = curspellang;
*/
#ifdef WITH_PARANOIA
      DpsViolationExit(-1, paran);
#endif
      return;
    }
  }
#ifdef WITH_PARANOIA
  DpsViolationExit(-1, paran);
#endif
  return;
}


static int CheckPrefix(const dpsunicode_t *word, DPS_AFFIX *Affix, DPS_AGENT *Indexer, int li, int pi, DPS_PSPELL *PS, DPS_PSPELL *FZ) {
  dpsunicode_t newword[2*MAXNORMLEN] = {0};
  int err, ls, rs, lres,rres, res;
  size_t newlen;
  DPS_AFFIX *CAffix = Indexer->Conf->Affixes.Affix;
#ifdef WITH_PARANOIA
  void *paran = DpsViolationEnter(paran);
#endif
  
  res = DpsUniStrNCaseCmp(word, Affix->repl, Affix->replen);
  if (res != 0) {
#ifdef WITH_PARANOIA
    DpsViolationExit(-1, paran);
#endif
    return res;
  }
  DpsUniStrCpy(newword, Affix->find);
  DpsUniStrCat(newword, word+Affix->replen);

  if (Affix->compile) {
    err = DpsUniRegComp(&(Affix->reg),Affix->mask);
    if(err){
      DpsUniRegFree(&(Affix->reg));
#ifdef WITH_PARANOIA
      DpsViolationExit(-1, paran);
#endif
      return (0);
    }
    Affix->compile = 0;
  }
  if((err=DpsUniRegExec(&(Affix->reg),newword))){
    DPS_SPELL **curspell;

    if((curspell = DpsFindWord(Indexer, newword, Affix->flag, PS, FZ))) {
    } 
    newlen = DpsUniLen(newword);
    ls = Indexer->Conf->Affixes.SuffixTree[li].Left[pi];
    rs = Indexer->Conf->Affixes.SuffixTree[li].Right[pi];
    while (ls >= 0 && ls <= rs) {
      CheckSuffix(newword, newlen, &CAffix[ls], &lres, Indexer, PS, FZ);
      if (rs > ls) {
	CheckSuffix(newword, newlen, &CAffix[rs], &rres, Indexer, PS, FZ);
      }
      ls++;
      rs--;
    }
  }
#ifdef WITH_PARANOIA
  DpsViolationExit(-1, paran);
#endif
  return 0;
}


__C_LINK DPS_SPELL ** __DPSCALL DpsNormalizeWord(DPS_AGENT * Indexer, DPS_WIDEWORD *wword, DPS_PSPELL *FZ) {
	size_t len;
	DPS_SPELL **forms;
	DPS_SPELL **cur;
	DPS_AFFIX * Affix;
	int ri, pi, ipi, lp, rp, cp, ls, rs, nlang = /*Indexer->SpellLang*/ -1 /*Indexer->spellang  FIXME: search form limit by lang  */ ;
	int li, li_from, li_to, lres, rres, cres = 0;
	dpsunicode_t *uword = wword->uword;
	DPS_PSPELL PS;

	len=DpsUniLen(uword);
	if (len < Indexer->WordParam.min_word_len 
		|| len > MAXNORMLEN
		|| len > Indexer->WordParam.max_word_len
		)
		return(NULL);
	
	PS.nspell = 0;
	forms = (DPS_SPELL **) DpsXmalloc(MAX_NORM*sizeof(DPS_SPELL *));
	if (forms == NULL) return NULL;
	PS.cur = cur = forms; *cur=NULL;

	ri = (int)((*uword) & 255);
	pi = (int)((uword[DpsUniLen(uword)-1]) & 255);
	if (nlang == -1) {
	  li_from = 0; li_to = Indexer->Conf->Spells.nLang;
	} else {
	  li_from  = nlang;
	  li_to = nlang + 1;
	}
	Affix = (DPS_AFFIX*)Indexer->Conf->Affixes.Affix;
	
	/* Check that the word itself is normal form */
	DpsFindWord(Indexer, uword, 0, &PS, FZ);

	/* Find all other NORMAL forms of the 'word' */
	
	for (ipi = 0; ipi <= pi; ipi += pi ? pi : 1) {

	  for (li = li_from; li < li_to; li++) {
	    /* check prefix */
	    lp = Indexer->Conf->Affixes.PrefixTree[li].Left[ri];
	    rp = Indexer->Conf->Affixes.PrefixTree[li].Right[ri];
	    while (lp >= 0 && lp <= rp) {
	      cp = (lp + rp) >> 1;
	      cres = 0;
	      if (PS.nspell < (MAX_NORM-1)) {
		cres = CheckPrefix(uword, &Affix[cp], Indexer, li, ipi, &PS, FZ);
	      }
	      if ((lp < cp) && ((cur - forms) < (MAX_NORM-1)) ) {
		lres = CheckPrefix(uword, &Affix[lp], Indexer, li, ipi, &PS, FZ);
	      }
	      if ( (rp > cp) && ((cur - forms) < (MAX_NORM-1)) ) {
		rres = CheckPrefix(uword, &Affix[rp], Indexer, li, ipi, &PS, FZ);
	      }
	      if (cres < 0) {
		rp = cp - 1;
		lp++;
	      } else if (cres > 0) {
		lp = cp + 1;
		rp--;
	      } else {
		lp++;
		rp--;
	      }
	    }

	    /* check suffix */
	    ls = Indexer->Conf->Affixes.SuffixTree[li].Left[ipi];
	    rs = Indexer->Conf->Affixes.SuffixTree[li].Right[ipi];
	    while (ls >= 0 && ls <= rs) {
	      CheckSuffix(uword, len, &Affix[ls], &lres, Indexer, &PS, FZ);
	      if ( rs > ls ) {
		CheckSuffix(uword, len, &Affix[rs], &rres, Indexer, &PS, FZ);
	      }
	      ls++;
	      rs--;
	    } /* end while */
	  
	  } /* for li */
	} /* for ipi */

	if(PS.nspell == 0) {
		DPS_FREE(forms);
		return NULL;
	}

	return forms;
}



void DpsSpellListFree(DPS_SPELLLIST *List){
	size_t i;

	for ( i = 0; i < List->nspell; i++) {
		DPS_FREE(List->Spell[i].word);
	}
	DPS_FREE(List->Spell);
	List->nspell = 0;
}

void DpsAffixListFree (DPS_AFFIXLIST *List) {
	size_t i;

	for (i = 0; i < List->naffixes; i++)
		if (List->Affix[i].compile == 0)
			DpsUniRegFree(&(List->Affix[i].reg));

	DPS_FREE(List->Affix);
	List->naffixes = 0;
}


static void DpsAllFormsWord (DPS_AGENT *Indexer, DPS_SPELL *word, DPS_WIDEWORDLIST *result, size_t order) {
  DPS_WIDEWORD w;
  size_t i;
  size_t naffixes = Indexer->Conf->Affixes.naffixes;
  DPS_AFFIX *Affix = (DPS_AFFIX *)Indexer->Conf->Affixes.Affix;
  int err;
  DPS_CHARSET *local_charset;
  DPS_CHARSET *sys_int;
  DPS_CONV fromuni;
  dpsunicode_t *r_word;
  
  local_charset = Indexer->Conf->lcs;
  if (local_charset == NULL) return;
  if (NULL==(sys_int=DpsGetCharSet("sys-int"))) return;
  DpsConvInit(&fromuni, sys_int, local_charset, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);

#ifdef DEBUG_UNIREG
  printf("start AllFormsWord\n");
#endif
  
  w.word = NULL;
  w.uword = NULL;

  r_word = DpsUniRDup(word->word);

  for (i = 0; i < naffixes; i++) {
    if ( (word->flag != NULL)
	 && (strcmp(word->lang, Affix[i].lang) == 0 )
	 && (strstr(word->flag, Affix[i].flag) != NULL)
	 ) {
      if (Affix[i].compile) {
	err = DpsUniRegComp(&(Affix[i].reg), Affix[i].mask);
	if(err){
	  DpsUniRegFree(&(Affix[i].reg));
	  DPS_FREE(r_word);
	  return;
	}
	Affix[i].compile = 0;
      }

      err = DpsUniRegExec(&(Affix[i].reg), r_word);
      if ( err
	   && (err = (Affix[i].type == 'p') ? (DpsUniStrNCaseCmp(r_word, Affix[i].find, Affix[i].findlen) == 0) :
	       (DpsUniStrBNCmp(r_word, Affix[i].find, Affix[i].findlen) == 0)
	       )
	   )  {
	
	w.len = DpsUniLen(r_word) - Affix[i].findlen + Affix[i].replen;
	if ( ( (w.word = DpsRealloc(w.word, 14 * w.len + 1)) == NULL) ||
	     ( (w.uword = DpsRealloc(w.uword, (w.len + 1) * sizeof(dpsunicode_t))) == NULL)) {
	  DPS_FREE(w.word); DPS_FREE(w.uword); DPS_FREE(r_word);
	  return;
	}

	bzero((void*)w.uword, (w.len + 1) * sizeof(dpsunicode_t));

	if (Affix[i].type == 'p') {
	  DpsUniStrCpy(w.uword, Affix[i].repl);
	  DpsUniStrCat(w.uword, &(r_word[Affix[i].findlen]));
	} else {
	  DpsUniStrNCpy(w.uword, r_word, DpsUniLen(r_word) - Affix[i].findlen);
	  DpsUniStrCat(w.uword, Affix[i].repl);
	}
	  
	DpsConv(&fromuni, w.word, 14 * w.len + 1, (char*)w.uword, sizeof(dpsunicode_t) * (w.len + 1));
	w.crcword = DpsStrHash32(w.word);
	w.order = order;
	w.count = 0;
	w.origin = DPS_WORD_ORIGIN_SPELL;
	DpsWideWordListAdd(result, &w, DPS_WWL_LOOSE);
      }

    }
  }
  DPS_FREE(w.word); DPS_FREE(w.uword); DPS_FREE(r_word);
}


__C_LINK DPS_WIDEWORDLIST * __DPSCALL DpsAllForms (DPS_AGENT *Indexer, DPS_WIDEWORD *wword) {
  DPS_SPELL **norm, **cur;
  DPS_WIDEWORDLIST *result, *syn = NULL;
  DPS_WIDEWORD w;
  size_t i, j;
  DPS_CHARSET *local_charset;
  DPS_CHARSET *sys_int;
  DPS_CONV fromuni;
  int sy   = DpsVarListFindInt(&Indexer->Vars, "sy", 1);
  int sp   = DpsVarListFindInt(&Indexer->Vars, "sp", 1);
  DPS_PSPELL PS, FZ;
  DPS_SPELL s_p, *p_sp = &s_p;
  
  PS.cur = NULL;

  FZ.nspell = 0;
  FZ.cur = &p_sp;
  s_p.word = NULL;

  if ((wword->ulen < Indexer->WordParam.min_word_len) || (wword->ulen == 1)) return NULL;
  local_charset = Indexer->Conf->lcs;
  if (local_charset == NULL) return NULL;
  if (NULL==(sys_int=DpsGetCharSet("sys-int"))) return NULL;
  DpsConvInit(&fromuni, sys_int, local_charset, Indexer->Conf->CharsToEscape, DPS_RECODE_HTML);

  if ((result = DpsXmalloc(sizeof(DPS_WIDEWORDLIST))) == NULL) {
    return NULL;
  }
  w.word = NULL;
  w.uword = NULL;

  if ((PS.cur = (DPS_SPELL **) DpsXmalloc(MAX_NORM*sizeof(DPS_SPELL *))) == NULL) return NULL;
  PS.nspell = 0;
  DpsWideWordListInit(result);
  cur = norm = DpsNormalizeWord(Indexer, wword, &FZ);

  if (cur != NULL) {
    while (*cur != NULL) {

      w.len = DpsUniLen((*cur)->word);
      if ( ( (w.word = DpsRealloc(w.word, 14 * w.len + 1)) == NULL) ||
	   ( (w.uword = DpsRealloc(w.uword, (w.len + 1) * sizeof(dpsunicode_t))) == NULL)) {
	DPS_FREE(w.word); DPS_FREE(w.uword); DPS_FREE(s_p.word);
	return NULL;
      }
      DpsUniStrRCpy(w.uword,(*cur)->word); 
      DpsConv(&fromuni, w.word, 14 * w.len + 1, (char*)w.uword, sizeof(w.uword[0]) * (w.len + 1));
      w.crcword = DpsStrHash32(w.word);
      w.order = wword->order;
      w.count = 0;
      w.origin = DPS_WORD_ORIGIN_SPELL;
      if (sp) { DpsWideWordListAdd(result, &w, DPS_WWL_LOOSE); }

      if (sy) syn = DpsSynonymListFind(&(Indexer->Conf->Synonyms), &w);

      if (syn != NULL)
	for(i = 0; i < syn->nwords; i++) {
	  DpsWideWordListAdd(result, &(syn->Word[i]), DPS_WWL_LOOSE);
	}
    
      if (sp) { DpsAllFormsWord(Indexer, *cur, result, wword->order); }
      if (syn != NULL) {
	for(i = 0; i < syn->nwords; i++) {
	  PS.nspell = 0;
	  DpsFindWord(Indexer, syn->Word[i].uword, 0, &PS, NULL);
	  for (j = 0; PS.cur[j] != NULL; j++) 
	    DpsAllFormsWord(Indexer, PS.cur[j], result, wword->order);
	}
      }
      if (Indexer->Flags.use_accentext) {
	dpsunicode_t *aw = DpsUniAccentStrip((*cur)->word);
	DPS_SPELL asp = **cur;
	size_t zz;
	for (zz = 0; zz < 2; zz++) {
	  if (DpsUniStrCmp(aw, (*cur)->word)) {
	    DpsUniStrRCpy(w.uword, aw); 
	    DpsConv(&fromuni, w.word, 14 * w.len + 1, (char*)w.uword, sizeof(w.uword[0]) * (w.len + 1));
	    w.crcword = DpsStrHash32(w.word);
	    w.origin = DPS_WORD_ORIGIN_ACCENT;
	    DpsWideWordListAdd(result, &w, DPS_WWL_LOOSE);
	    asp.word = aw;
	    if (sp) DpsAllFormsWord(Indexer, &asp, result, wword->order);
	  }
	  DPS_FREE(aw);
	  if (zz == 0) {
	    aw = DpsUniGermanReplace((*cur)->word);
	  }
	}
      }
      cur++;
    }
  } else if (/*FZ.nspell > 0 && */FZ.nspell > Indexer->WordParam.min_word_len) {

      w.len = DpsUniLen(s_p.word);
      if ( ( (w.word = DpsRealloc(w.word, 14 * w.len + 1)) == NULL) ||
	   ( (w.uword = DpsRealloc(w.uword, (w.len + 1) * sizeof(dpsunicode_t))) == NULL)) {
	DPS_FREE(w.word); DPS_FREE(w.uword); DPS_FREE(s_p.word);
	return NULL;
      }
      DpsUniStrRCpy(w.uword, s_p.word); 
      DpsConv(&fromuni, w.word, 14 * w.len + 1, (char*)w.uword, sizeof(w.uword[0]) * (w.len + 1));
      w.crcword = DpsStrHash32(w.word);
      w.order = wword->order;
      w.count = 0;
      w.origin = DPS_WORD_ORIGIN_SPELL;
      if (sp) { DpsWideWordListAdd(result, &w, DPS_WWL_LOOSE); }

      if (sy) syn = DpsSynonymListFind(&(Indexer->Conf->Synonyms), &w);

      if (syn != NULL)
	for(i = 0; i < syn->nwords; i++) {
	  DpsWideWordListAdd(result, &(syn->Word[i]), DPS_WWL_LOOSE);
	}
    
      if (sp) { DpsAllFormsWord(Indexer, p_sp, result, wword->order); }
      if (syn != NULL) {
	for(i = 0; i < syn->nwords; i++) {
	  PS.nspell = 0;
	  DpsFindWord(Indexer, syn->Word[i].uword, 0, &PS, NULL);
	  for (j = 0; PS.cur[j] != NULL; j++) 
	    DpsAllFormsWord(Indexer, PS.cur[j], result, wword->order);
	}
      }
      if (Indexer->Flags.use_accentext) {
	dpsunicode_t *aw = DpsUniAccentStrip(p_sp->word);
	DPS_SPELL asp = *p_sp;
	size_t zz;
	for (zz = 0; zz < 2; zz++) {
	  if (DpsUniStrCmp(aw, p_sp->word)) {
	    DpsUniStrRCpy(w.uword, aw); 
	    DpsConv(&fromuni, w.word, 14 * w.len + 1, (char*)w.uword, sizeof(w.uword[0]) * (w.len + 1));
	    w.crcword = DpsStrHash32(w.word);
	    w.origin = DPS_WORD_ORIGIN_ACCENT; 
	    DpsWideWordListAdd(result, &w, DPS_WWL_LOOSE);
	    asp.word = aw;
	    if (sp) DpsAllFormsWord(Indexer, &asp, result, wword->order);
	  }
	  DPS_FREE(aw);
	  if (zz == 0) {
	    aw = DpsUniGermanReplace(p_sp->word);
	  }
	}
      }
  } else {

    /*DpsWideWordListAdd(result, wword);*/
    if (sy) syn = DpsSynonymListFind(&(Indexer->Conf->Synonyms), wword);

    if (syn != NULL) {
      for(i = 0; i < syn->nwords; i++) {
	DpsWideWordListAdd(result, &(syn->Word[i]), DPS_WWL_LOOSE);
      }
    
      for(i = 0; i < syn->nwords; i++) {
	PS.nspell = 0;
	DpsFindWord(Indexer, syn->Word[i].uword, 0, &PS, NULL);
	for (j = 0; PS.cur[j] != NULL; j++) 
	  DpsAllFormsWord(Indexer, PS.cur[j], result, wword->order);
      }
    }
  }
  DPS_FREE(w.word); DPS_FREE(w.uword);
  DPS_FREE(norm);
  DPS_FREE(PS.cur);
  DPS_FREE(s_p.word);

  return result;
}
