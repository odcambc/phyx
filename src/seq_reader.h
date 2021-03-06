#ifndef _SEQ_READER_H_
#define _SEQ_READER_H_

#include "sequence.h"

string get_filetype_string (int const& ft);
int test_seq_filetype_stream (istream & stri, string & retstring);
int test_char_filetype_stream (istream & stri, string & retstring);
bool read_next_seq_from_stream (istream & stri, int ftype, string & retstring,
    Sequence & seq);
bool read_next_seq_char_from_stream (istream & stri, int ftype,
    string & retstring, Sequence & seq);
void get_nexus_dimensions (string & filen, int & numTaxa, int & numChar);

// deprecated
int test_seq_filetype (string filen);
//bool read_fasta_file (string filen, vector <Sequence>& seqs);
//bool read_phylip_file (string filen, vector <Sequence>& seqs);
//bool read_phylip_file_strec (string filen, vector <Sequence>& seqs);
//bool read_nexus_seqs_file(string filen, vector <Sequence>& seqs);

#endif /* _SEQ_READER_H_ */
