/*
 * main_seqgen.cpp
 *
 *  Created on: Jun 23, 2015
 *      Author: joe
 */


#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <cstring>
#include <getopt.h>

using namespace std;

#include "seqgen.h"
#include "utils.h"
#include "tree.h"
#include "tree_reader.h"


void print_help() {
    cout << "Basic sequence simulator under the GTR model." << endl;
    cout << "This will take fasta, fastq, phylip, and nexus inputs." << endl;
    cout << endl;
    cout << "Usage: pxseqgen [OPTION]... " << endl;
    cout << endl;
    cout << " -t, --treef=FILE       input treefile, stdin otherwise" << endl;
    cout << " -o, --outf=FILE        output seq file, stout otherwise" << endl;
    cout << " -l, --length=INT       length of sequences to generate. default is 1000" << endl;
    cout << " -b, --basef=Input      comma-delimited base freqs in order: A,C,G,T. default is equal" << endl;
    cout << " -g, --gamma=INT        gamma shape value. default is no rate variation" << endl;
    cout << " -i, --pinvar=FLOAT     proportion of invariable sites. default is 0.0" << endl;
    cout << " -r, --ratemat=Input    comma-delimited input values for rate matrix. default is JC69" << endl;
    cout << "                          order: A<->C,A<->G,A<->T,C<->G,C<->T,G<->T" << endl;
    cout << " -n, --nreps=INT        number of replicates" << endl;
    cout << " -x, --seed=INT         random number seed, clock otherwise" << endl;
    cout << " -a, --ancestors        print the ancestral node sequences. default is no" << endl;
    cout << "                          use -p for the nodes labels" << endl;
    cout << " -p, --printnodelabels  print newick with internal node labels. default is no" << endl;
    cout << " -m, --multimodel=Input specify multiple models across tree" << endl;
    cout << "                          input is as follows:" << endl; 
    cout << "                            A<->C,A<->G,A<->T,C<->G,C<->T,G<->T,Node#,A<->C,A<->G,A<->T,C<->G,C<->T,G<->T" << endl;
    cout << "                            EX:.3,.3,.3,.3,.3,1,.3,.3,.2,.5,.4" << endl;
    cout << " -k, --rootseq=STRING   set root sequence. default is random (from basefreqs)" << endl;
    cout << "     --help             display this help and exit" << endl;
    cout << "     --version          display version and exit" << endl;
    cout << endl;
    cout << "Report bugs to: <https://github.com/FePhyFoFum/phyx/issues>" << endl;
    cout << "phyx home page: <https://github.com/FePhyFoFum/phyx>" << endl;
}

string versionline("pxseqgen 0.1\nCopyright (C) 2015 FePhyFoFum\nLicense GPLv2\nwritten by Joseph F. Walker, Joseph W. Brown, Stephen A. Smith (blackrim)");

static struct option const long_options[] =
{
    {"treef", required_argument, NULL, 't'},
    {"outf", required_argument, NULL, 'o'},
    {"length", required_argument, NULL, 'l'},
    {"basef", required_argument, NULL, 'b'},
    {"gamma", required_argument, NULL, 'g'},
    {"pinvar", required_argument, NULL, 'i'},
    {"ratemat", required_argument, NULL, 'r'},
    {"nreps", required_argument, NULL, 'n'},
    {"seed", required_argument, NULL, 'x'},
    {"ancestors", no_argument, NULL, 'a'},
    {"printnodelabels", no_argument, NULL, 'p'},
    {"multimodel", required_argument, NULL, 'm'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {NULL, 0, NULL, 0}
};

int main(int argc, char * argv[]) {
    bool outfileset = false;
    bool fileset = false;
    bool printpost = false;
    bool showancs = false;
    bool mm = false;
    float pinvar = 0.0;
    string yorn = "n";
    int seqlen = 1000;
    string infreqs;
    string inrates;
    string holdrates;
    string ancseq;
    char * outf;
    char * treef;
    vector <double> basefreq(4, 0.25);
    vector <double> userrates;
    vector <double> multirates(4, 0.25);
    int nreps = 1; // not implemented at the moment
    int seed = -1;
    int numpars = 0;
    float alpha = -1.0;
    vector< vector <double> > rmatrix(4, vector<double>(4, 0.33));
    for (unsigned int i = 0; i < rmatrix.size(); i++) {
        for (unsigned int j = 0; j < rmatrix.size(); j++) {
            if (i == j) { // Fill Diagonal
                rmatrix[i][j] = -0.99;
            }
        }
    }
    while (1) {
        int oi = -1;
        int c = getopt_long(argc, argv, "t:o:l:b:g:i:r:n:x:apm:k:hV", long_options, &oi);
        if (c == -1) {
            break;
        }
        switch(c) {
            case 't':
                fileset = true;
                treef = strdup(optarg);
                check_file_exists(treef);
                break;
            case 'o':
                outfileset = true;
                outf = strdup(optarg);
                break;
            case 'b':
                infreqs = strdup(optarg);
                basefreq = parse_double_comma_list(infreqs);
                if (basefreq.size() != 4) {
                    cout << "Error: must provide 4 base frequencies (" << basefreq.size()
                        << " provided). Exiting." << endl;
                    exit(0);
                }
                if (!essentially_equal(sum(basefreq), 1.0)) {
                    cout << "Error: base frequencies must sum to 1.0. Exiting." << endl;
                    exit(0);
                }
                break;
            case 'l':
                seqlen = atoi(strdup(optarg));
                break;
            case 'a':
                showancs = true;
                break;
            case 'r':
                inrates = strdup(optarg);
                userrates = parse_double_comma_list(inrates);
                
                // NOTE: will have to alter this check for a.a., non-reversible, etc.
                if (userrates.size() != 6) {
                    cout << "Error: must provide 6 substitution parameters. " <<
                        "Only " << userrates.size() << " provided. Exiting." << endl;
                    exit(0);
                }
                // NOTE: this uses order: A,T,C,G for matrix, but
                //       A<->C,A<->G,A<->T,C<->G,C<->T,G<->T for subst. params.
                rmatrix[0][2] = userrates[0];
                rmatrix[2][0] = userrates[0];
                rmatrix[0][3] = userrates[1];
                rmatrix[3][0] = userrates[1];
                rmatrix[0][1] = userrates[2];
                rmatrix[1][0] = userrates[2];
                rmatrix[2][3] = userrates[3];
                rmatrix[3][2] = userrates[3];
                rmatrix[1][2] = userrates[4];
                rmatrix[2][1] = userrates[4];
                rmatrix[1][3] = userrates[5];
                rmatrix[3][1] = userrates[5];
                rmatrix[0][0] = (userrates[0]+userrates[1]+userrates[2]) * -1;
                rmatrix[1][1] = (userrates[2]+userrates[4]+userrates[5]) * -1;
                rmatrix[2][2] = (userrates[0]+userrates[3]+userrates[4]) * -1;
                rmatrix[3][3] = (userrates[1]+userrates[3]+userrates[5]) * -1;
                /*//Turn on to check matrix
                for (unsigned int i = 0; i < rmatrix.size(); i++) {
                   for (unsigned int j = 0; j < rmatrix.size(); j++) {
                      cout << rmatrix[i][j] << " ";
                   }
                    cout << "\n";
                }*/
                break;
            case 'n':
                nreps = atoi(strdup(optarg));
                break;
            case 'x':
                seed = atoi(strdup(optarg));
                break;
            case 'g':
                alpha = atof(strdup(optarg));
                break;
            case 'i':
                pinvar = atof(strdup(optarg));
                break;
            case 'p':
                printpost = true;
                break;
            case 'm':
                mm = true;
                holdrates = strdup(optarg);
                multirates = parse_double_comma_list(holdrates);
                numpars = multirates.size();
                if ((numpars - 6) % 7 != 0) {
                    cout << "Error: must provide 6 background substitution "
                        << "parameters and 7 values (1 node id + 6 subst. par.) "
                        << "for each piecewise model. Exiting." << endl;
                    exit(0);
                }
                break;
            case 'k':
                ancseq = strdup(optarg);
                break;
            case 'h':
                print_help();
                exit(0);
            case 'V':
                cout << versionline << endl;
                exit(0);
            default:
                print_error(argv[0], (char)c);
                exit(0);
        }
    }
    
    istream* pios;
    ostream* poos;
    ifstream* fstr;
    ofstream* ofstr;
    if (outfileset == true) {
        ofstr = new ofstream(outf);
        poos = ofstr;
    } else {
        poos = &cout;
    }
    if (fileset == true) {
        fstr = new ifstream(treef);
        pios = fstr;
    } else {
        pios = &cin;
    }
    
    
    /*
     * Default Base Frequencies and Rate Matrix
     *
     */
    //vector <double> basefreq(4, 0.0);
    //basefreq[0] = .25;
    //basefreq[1] = .25;
    //basefreq[2] = .25;
    //basefreq[3] = 1.0 - basefreq[0] - basefreq[1] - basefreq[2];
    /*    
    vector< vector <double> > rmatrix(4, vector<double>(4, 0.33));
    for (unsigned int i = 0; i < rmatrix.size(); i++) {
        for (unsigned int j = 0; j < rmatrix.size(); j++) {
            if (i == j) {//Fill Diagnol
                rmatrix[i][j] = -1.0;
            }
        }
    }
    */
    string retstring;
    int ft = test_tree_filetype_stream(*pios, retstring);
    if (ft != 0 && ft != 1) {
        cerr << "this really only works with nexus or newick" << endl;
        exit(0);
    }
    
    // allow > 1 tree in input. passing but not yet using nreps
    int treeCounter = 0;
    bool going = true;
    if (ft == 1) { // newick. easy
        Tree * tree;
        while (going) {
            tree = read_next_tree_from_stream_newick (*pios, retstring, &going);
            if (tree != NULL) {
                //cout << "Working on tree #" << treeCounter << endl;
                SequenceGenerator SGen(seqlen, basefreq, rmatrix, tree, showancs,
                    nreps, seed, alpha, pinvar, ancseq, printpost, multirates, mm);
                vector <Sequence> seqs = SGen.get_sequences();
                for (unsigned int i = 0; i < seqs.size(); i++) {
                    Sequence seq = seqs[i];
                    (*poos) << ">" << seq.get_id() << endl;
                    //cout << "Here" << endl;
                    (*poos) << seq.get_sequence() << endl;
                }
                delete tree;
                treeCounter++;
            }
        }
    } else if (ft == 0) { // Nexus. need to worry about possible translation tables
        map <string, string> translation_table;
        bool ttexists;
        ttexists = get_nexus_translation_table(*pios, &translation_table, &retstring);
        Tree * tree;
        while (going) {
            tree = read_next_tree_from_stream_nexus(*pios, retstring, ttexists,
                &translation_table, &going);
            if (going == true) {
                cout << "Working on tree #" << treeCounter << endl;
                SequenceGenerator SGen(seqlen, basefreq, rmatrix, tree, showancs,
                    nreps, seed, alpha, pinvar, ancseq, printpost, multirates, mm);
                vector <Sequence> seqs = SGen.get_sequences();
                for (unsigned int i = 0; i < seqs.size(); i++) {
                    Sequence seq = seqs[i];
                    (*poos) << ">" << seq.get_id() << endl;
                    (*poos) << seq.get_sequence() << endl;
                }
                delete tree;
                treeCounter++;
            }
        }
    }
    
    return EXIT_SUCCESS;
}
