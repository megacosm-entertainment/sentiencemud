#!/bin/sh

DaySuffix() {
    if [ "x`date +%-d | cut -c2`x" = "xx" ]
    then
        DayNum=`date +%-d`
    else
        DayNum=`date +%-d | cut -c2`
    fi

    CheckSpecialCase=`date +%-d`
    case $DayNum in
    0 )
      echo "th" ;;
    1 )
      if [ "$CheckSpecialCase" = "11" ]
      then
        echo "th"
      else
        echo "st"
      fi ;;
    2 )
      if [ "$CheckSpecialCase" = "12" ]
      then
        echo "th"
      else
        echo "nd"
      fi ;;
    3 )
      if [ "$CheckSpecialCase" = "13" ]
      then
        echo "th"
      else
        echo "rd"
      fi ;;
    [4-9] )
      echo "th" ;;
    * )
      return 1 ;;
    esac
}

# Using consolidated date command from chris_l
# Also using %-d instead of %d so it doesn't pad with 0's
date "+%A, %B %-d`DaySuffix` %Y at %r"
