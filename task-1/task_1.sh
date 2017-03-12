#!/bin/bash

SERVE=0
HOST=localhost
PORT=6666

help() {
    echo "Крестики-нолики"
    echo "Запуск: $EXENAME [ --server | HOST ] [ --port PORT ]"
    exit 0
}

init() {
	row1[1]=' '
	row1[2]=' '
	row1[3]=' '

	row2[1]=' '
	row2[2]=' '
	row2[3]=' '

	row3[1]=' '
	row3[2]=' '
	row3[3]=' '

	showField
}

showField() {
	clear 

	echo " ${row1[1]} │ ${row1[2]} │ ${row1[3]} "
	echo "───┼───┼───"
	echo " ${row2[1]} │ ${row2[2]} │ ${row2[3]} "
	echo "───┼───┼───"
	echo " ${row3[1]} │ ${row3[2]} │ ${row3[3]} "
}

parse() {
	coords=(${1//,/ })
}

addMark() {
	x=$1
	y=$2
	mark=$3
	result=0

	case $x in 
		"1")
			current=${row1[$y]}
			if [[ $current == " " ]];
				then row1[$y]=$mark
				else result=1
			fi 
			;;
		"2")
			current=${row2[$y]}
			if [[ ${current} == " " ]]
				then row2[$y]=$mark
				else result=1
			fi 
			;;
		"3")
			current=${row3[$y]}
			if [[ ${current} == " " ]]
				then row3[$y]=$mark
				else result=1
			fi 
			;;
	esac

	return $result
}

checkFinish() {
	winner=""
	strs=(
		${row1[1]}${row1[2]}${row1[3]} 
		${row2[1]}${row2[2]}${row2[3]} 
		${row3[1]}${row3[2]}${row3[3]} 
		${row1[1]}${row2[2]}${row3[3]} 
		${row3[1]}${row2[2]}${row1[3]} 
		${row1[1]}${row2[1]}${row3[1]} 
		${row1[2]}${row2[2]}${row3[2]} 
		${row1[3]}${row2[3]}${row3[3]}
		)

	for str in ${strs[@]}; do
		if [[ "$str" == "xxx" ]]; then
			winner="x"
			break
		fi

		if [[ "$str" == "ooo" ]]; then
			winner="o"
			break
		fi
	done

	if [[ $winner == $myMark ]]; then
		echo "Вы выиграли"
		exit 0
	fi

	if [[ $winner == $theirMark ]]; then
		echo "Вы проиграли"
		exit 0
	fi

	str=${row1[1]}${row1[2]}${row1[3]}${row2[1]}${row2[2]}${row2[3]}${row3[1]}${row3[2]}${row3[3]}
	if ! [[ $str =~ \  ]]; then
		echo "Не осталось ходов"
		exit 0
	fi
}

clearStdin() {
	while read -t 0 notused; do
   		read input
	done
}

waitForTurn() {
	echo "Ожидание хода противника..."

	stty -echo # отключаем отображение ввода на время ожидания
	read -u ${COPROC[0]} THEIRMOVE
	stty echo

	parse $THEIRMOVE 
	addMark ${coords[0]} ${coords[1]} $theirMark

	showField

	echo "Противник сходил: $THEIRMOVE"
	echo "Ваш ход"
}

makeTurn() {
	clearStdin # очистка текста, который ввел пользователь, пока ожидал своего хода

	while true; do
		read MYMOVE
		if [[ $MYMOVE =~ ^[1-3],[1-3]$ ]]; then
			parse $MYMOVE
			addMark ${coords[0]} ${coords[1]} $myMark
			if [[ $? == "0" ]]
				then break
				else echo "Неверные данные"
			fi
		else 
			echo "Неверные данные"
		fi

	done

	showField

	echo $MYMOVE >&${COPROC[1]}
}

while (( $# > 0 )); do
    case $1 in
        "--server")
            SERVE=1
            ;;
        "--port")
            if (( $# < 2 )); then 
            	help
            fi
            PORT=$2
            shift
            ;;
        -*)
            echo "Неизвестная опция $1"
            help
            ;;
        *)
            if [ -n $HOST ]; then
            	help
            fi
            HOST=$1
            ;;
    esac
    shift
done

init

if (( SERVE == 1 )); then
    coproc nc -l -p $PORT
    myMark="o"
    theirMark="x"
	waitForTurn
else
    coproc nc $HOST $PORT
    myMark="x"
    theirMark="o"
    echo "Ваш ход первый."
fi

while true; do
	makeTurn
	checkFinish

	waitForTurn
	checkFinish
done
