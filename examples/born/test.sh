#!/bin/bash

if [[ $1 = "" ]]; then
    echo "Please use \"make test\" to run the tests."
    exit
fi

if [[ $2 = "" ]]; then
    echo "Please use \"make test\" to run the tests."
    exit
fi

logfile=TESTRESULTS.log
nettime=0
vsmall=0.000000001000

input=( apbs-mol-auto apbs-smol-auto  apbs-mol-parallel apbs-smol-parallel apbs-apolar )

results=( -2.297735411962E+02 -2.290122772380E+02 -2.304915804304E+02 -2.293871983649E+02 )

# Initialize the results file

date=`date`
echo "Date     : ${date}" >> $logfile
echo "Directory: born" >> $logfile
echo "Results  :" >> $logfile

# For each file in the directory, run APBS and get the value

for i in 0 1 2 3 4
do
  echo "----------------------------------------"
  echo "Testing input file ${input[i]}.in"
  echo ""

  procs=`grep -m 1 pdime ${input[i]}.in | awk '{print $2*$3*$4}'`
  if [[  $procs != "" ]]; then
      # Split the files

      echo "Splitting the input file into $procs separate files using the"
      echo "inputgen.py utility..."
      echo ""

      python $2 --split ${input[i]}.in
      
      j=0
      answer=0
      starttime=`date +%s`
      while [ $j -lt $procs ]
      do
        $1 ${input[i]}-PE$j.in > ${input[i]}-PE$j.out
        a=`grep "Global net ELEC" ${input[i]}-PE$j.out | awk '{print $6}'`
        echo "Processor $j result: $a"
        conv=`printf "%.12f" $a`
        answer=`echo "scale=12; $answer+$conv" | bc`
        echo ""
        let j=$j+1
      done
      answer=`printf "%.12E" $answer`

  else
      starttime=`date +%s`
      $1 ${input[i]}.in > ${input[i]}.out 
      answer=`grep "Global net ELEC" ${input[i]}.out | awk '{print $6}'`
  fi


  echo "Global net energy: $answer"
  sync
  
  fanswer=`printf "%.12f" $answer`
  fexpected=`printf "%.12f" ${results[i]}`
  r=`echo "scale=12;if($fanswer>($fexpected-$vsmall) && $fanswer<($fexpected+$vsmall))r=1;if($fanswer == $fexpected)r=2;r" | bc`

  case "$r" in
      2)  echo "*** PASSED ***"
          echo "           ${input[i]}.in: PASSED ($answer)" >> $logfile ;;
      1)  echo "*** PASSED (with rounding error - see log) ***"
          echo "           ${input[i]}.in: PASSED with rounding error ($answer; expected ${results[i]})" >> $logfile ;;
      *)  error=`echo "scale=12;e=($fanswer - $fexpected)*100.0/$fexpected;;if(e<0)e=e*-1;e" | bc`
          ferror=`printf "%.2f" $error`
          echo "*** FAILED ***"
          echo "   APBS returned $answer"
          echo "   Expected result is ${results[i]} ($ferror% error)"
          echo "           ${input[i]}.in: FAILED ($answer; expected ${results[i]}; $ferror% error)" >> $logfile ;;
  esac
  
  endtime=`date +%s`
  let elapsed=$endtime-$starttime
  let nettime=$nettime+$elapsed
  echo "Total elapsed time: $elapsed seconds"
  echo "----------------------------------------"

done

echo "Test results have been logged to ${logfile}."
echo "Total time for this directory: ${nettime} seconds."

echo "Time     : ${nettime} seconds" >> $logfile
echo "" >> $logfile
