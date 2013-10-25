
#include "tn93_shared.h"

#ifdef  _OPENMP
  #include "omp.h"
#endif

#include "argparse.hpp"


using namespace std;
using namespace argparse;

#define HISTOGRAM_BINS 200
#define HISTOGRAM_SLICE ((double)HISTOGRAM_BINS)

char sep = ':';

//---------------------------------------------------------------


int main (int argc, const char * argv[])
{
    args_t args = args_t (argc, argv);

    long     firstSequenceLength = 0;


    StringBuffer names,
                 sequences;

    Vector       nameLengths,
                 seqLengths,
                 counts;



    char automatonState = 0;
    // 0 - between sequences
    // 1 - reading sequence name
    // 2 - reading sequence

    nameLengths.appendValue (0);
    seqLengths.appendValue (0);
    initAlphabets();
    if (readFASTA (args.input1, automatonState, names, sequences, nameLengths, seqLengths, firstSequenceLength, false, &counts, args.counts_in_name) == 1)
        return 1;


    unsigned long seqLengthInFile1 = seqLengths.length()-1,
                  seqLengthInFile2 = 0;

    if (args.input2) {
        automatonState = 0;
        if (readFASTA (args.input2, automatonState, names, sequences, nameLengths, seqLengths, firstSequenceLength, false, &counts, args.counts_in_name) == 1)
            return 1;
        seqLengthInFile2 = seqLengths.length()-1-seqLengthInFile1;
    }

    unsigned long sequenceCount = seqLengths.length()-1,
                  pairwise      = args.input2 ? seqLengthInFile2*seqLengthInFile1 : (sequenceCount-1) * (sequenceCount) / 2;

    if (! args.quiet )
      if (args.input2 == NULL)
          cerr << "Read " << sequenceCount << " sequences of length " << firstSequenceLength << endl << "Will perform " << pairwise << " pairwise distance calculations";
      else
          cerr << "Read " << seqLengthInFile1 << " sequences from file 1 and " << seqLengthInFile2 << " sequences from file 2 of length " << firstSequenceLength << endl << "Will perform " << pairwise << " pairwise distance calculations";

    double percentDone = 0.,
           max         = 0.0,
           mean        = 0.0;




    long * randFlag = NULL;
    long * randSeqs = NULL;

    if (args.do_bootstrap) {
        if (args.input2 == NULL) {
            if (! args.quiet )
              cerr << endl << "Randomizing site order..." << endl;
            randFlag = new long [firstSequenceLength];
            bool   *included = new bool [firstSequenceLength];
            for (long k = 0; k < firstSequenceLength; k++) {
                included[k] = false;
            }
            init_genrand (time(NULL) + getpid ());

            long normalizer = RAND_RANGE / firstSequenceLength,
                 max_site   = 0;


            for (long i = 0; i < firstSequenceLength; i++) {
                randFlag[i] = genrand_int32 () / normalizer;
                if (! args.quiet)
                  cerr << randFlag[i] << " ";
                included[randFlag[i]] = true;
                if (randFlag[i] > max_site) max_site = randFlag[i];
            }

            long total_resampled = 0;
            for (long k = 0; k < firstSequenceLength; k++) {
                total_resampled += included[k];
            }

            delete [] included;
            if (! args.quiet )
              cerr << endl << "Unique sites included in the resampled order " << total_resampled << endl;
        } else {
              randSeqs = new long [sequenceCount];
              for (unsigned long k = 0; k < sequenceCount; k++) {
                  randSeqs [k] = k;
              }

              init_genrand (time(NULL) + getpid ());


              for (unsigned long i = 0; i < sequenceCount; i++) {
                  long id = genrand_int32 () / (RAND_RANGE / (sequenceCount-i)),
                       t = randSeqs[i+id];
                  randSeqs[i+id]= randSeqs[i];
                  randSeqs[i] = t;
              }
              if (! args.quiet )
                cerr << endl << "Randomized sequence assignment to datasets " << endl;
        }
    }


    if (! args.quiet )
      cerr << endl << "Progress: ";

    long pairIndex  = 0,
         foundLinks = 0;

    double *distanceMatrix = NULL;

    if (args.format != hyphy) {
        if (args.do_count == false) {
            if (args.format == csv)
                fprintf (args.output, "ID1,ID2,Distance\n");
            else
                fprintf (args.output, "Seq1,Seq2,Distance\n");
        }
    }
    else {
        distanceMatrix = new double [sequenceCount*sequenceCount];
        for (unsigned long i = 0; i < sequenceCount*sequenceCount; i++)
            distanceMatrix[i] = 100.;
        for (unsigned long i = 0; i < sequenceCount; i++)
            distanceMatrix[i*sequenceCount+i] = 0.;
    }

    long upperBound = args.input2 ? seqLengthInFile1 : sequenceCount,
         skipped_comparisons = 0;
         
    
    unsigned long global_hist [HISTOGRAM_BINS];
    for (unsigned long k = 0; k < HISTOGRAM_BINS; k++) {
      global_hist[k] = 0;
    }
  
    int resolutionOption;
  switch (args.ambig) {
    case resolve:
      resolutionOption = RESOLVE;
      break;
    case average:
      resolutionOption = AVERAGE;
      break;
    case skip:
      resolutionOption = SKIP;
      break;
    case gapmm:
      resolutionOption = GAPMM;
      break;
      
  }
  
  time_t before, after;
  time (&before); 
  double weighted_counts = 0.;

    #pragma omp parallel shared(skipped_comparisons, resolutionOption, foundLinks,pairIndex,sequences,seqLengths,sequenceCount,firstSequenceLength,args, nameLengths, names, pairwise, percentDone,cerr,max,randFlag,distanceMatrix, upperBound, seqLengthInFile1, seqLengthInFile2, mean, randSeqs, weighted_counts)
    
    {
    
      unsigned long histogram_counts [HISTOGRAM_BINS];
      for (unsigned long k = 0; k < HISTOGRAM_BINS; k++) {
        histogram_counts[k] = 0;
      }
    
      #pragma omp for schedule (dynamic)
      for (long seq1 = 0; seq1 < upperBound; seq1 ++)
      {
          long mapped_id = randSeqs?randSeqs[seq1]:seq1;

          char *n1 = stringText (names, nameLengths, mapped_id),
                *s1 = stringText(sequences, seqLengths, mapped_id);

          long lowerBound = args.input2 ? seqLengthInFile1 : seq1 +1,
               compsSkipped      = 0,
               local_links_found = 0,
               instances1 = counts.value (mapped_id);

          double local_max = 0.,
                 local_sum = 0.,
                 local_weighted = 0.;

          for (unsigned long seq2 = lowerBound; seq2 < sequenceCount; seq2 ++)
          {
              long mapped_id2 = randSeqs?randSeqs[seq2]:seq2,
                   instances2 = counts.value (mapped_id2);
                   
              double thisD = computeTN93(s1, stringText(sequences, seqLengths, mapped_id2), firstSequenceLength, resolutionOption, randFlag, args.overlap, histogram_counts, HISTOGRAM_SLICE, HISTOGRAM_BINS, instances1, instances2);

              if (thisD >= -1.e-10 && thisD <= args.distance) {
                  local_links_found ++;
                  //char *s2 = stringText(sequences, seqLengths, seq1);
                  if (!args.do_count) {
                      if (args.format == csv) {
                          #pragma omp critical
                          fprintf (args.output,"%s,%s,%g\n", n1, stringText (names, nameLengths, mapped_id2), thisD);
                      } else {
                          if (args.format == csvn) {
                              #pragma omp critical
                              fprintf (args.output,"%ld,%ld,%g\n", mapped_id, mapped_id2, thisD);

                          } else {
                              distanceMatrix[mapped_id*sequenceCount+mapped_id2] = thisD;
                              distanceMatrix[mapped_id2*sequenceCount+mapped_id] = thisD;
                          }
                      }
                  }
              }
              if (thisD <= -0.5) {
                  compsSkipped ++;
              }
              else {
                  long weighted_count = instances1*instances2;
                  local_sum += thisD * (weighted_count);
                  local_weighted += weighted_count;
                  if (thisD > local_max) {
                      local_max = thisD;
                  }
              }


          }
          #pragma omp critical
          {
              foundLinks += local_links_found;
              skipped_comparisons += compsSkipped;
              if (local_max > max) {
                  max = local_max;
              }
              pairIndex += args.input2 == NULL ? (sequenceCount - seq1 - 1) : seqLengthInFile2;
              mean += local_sum;
              weighted_counts += local_weighted;
          }
           

          if (! args.quiet && (pairIndex * 100. / pairwise - percentDone > 0.1 || seq1 == (long)sequenceCount - 1))
          {
              #pragma omp critical
              {
              time (&after);
              percentDone = pairIndex * 100. / pairwise;
              cerr << "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\bProgress:" 
                   << setw (8) << percentDone << "% (" << setw(8) << foundLinks << " links found, "
                   << setw (12) << std::setprecision(3) << pairIndex/difftime (after,before) << " evals/sec)";
              
              after = before;
              }
          }
      }
      #pragma omp critical
      {
        for (unsigned long k = 0; k < HISTOGRAM_BINS; k++) {
          global_hist[k] += histogram_counts[k];
        }
      }
      
    }

    if (args.do_count == false && args.format == hyphy)
    {
        fprintf (args.output, "{");
        for (unsigned long seq1 = 0; seq1 < sequenceCount; seq1 ++)
        {
            fprintf (args.output, "\n{%g", distanceMatrix[seq1*sequenceCount]);
            for (unsigned long seq2 = 1; seq2 < sequenceCount; seq2++)
            {
                fprintf (args.output, ",%g", distanceMatrix[seq1*sequenceCount+seq2]);
            }
            fprintf (args.output, "}");
        }
        fprintf (args.output, "\n}");

        delete [] distanceMatrix;
    }

    cerr << endl;
    
    ostream * outStream = &cout;
    
    if (args.output == stdout) {
      outStream = &cerr;
    }
    
    (*outStream) << "{" << endl << '\t' << "\"Actual comparisons performed\" :" << pairwise-skipped_comparisons << ',' << endl;
    (*outStream) << "\t\"Total comparisons possible\" : " << pairwise << ',' << endl;
    (*outStream) << "\t\"Links found\" : " << foundLinks << ',' << endl;
    (*outStream) << "\t\"Maximum distance\" : " << max << ',' << endl;
    (*outStream) << "\t\"Mean distance\" : " << mean/weighted_counts << ',' << endl;
    (*outStream) << "\t\"Histogram\" : [";
    for (unsigned long k = 0; k < HISTOGRAM_BINS; k++) {
      if (k) {
        (*outStream) << ',';
      }
      (*outStream) << '[' << (k+1.) / HISTOGRAM_BINS << ',' << global_hist[k] << ']';
    }
    (*outStream) << "]" << endl;
    
    (*outStream) << '}' << endl;
    
    if (args.do_count) {
        fprintf (args.output, "Found %ld links among %ld pairwise comparisons\n", foundLinks, pairwise-skipped_comparisons);
    }

    if (randFlag)
        delete [] randFlag;

    if (randSeqs)
        delete [] randSeqs;

    return 0;

}

