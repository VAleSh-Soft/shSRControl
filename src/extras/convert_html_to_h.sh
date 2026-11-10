#!/bin/bash
python3 temp/html_to_h.py temp/i_page.html -n INDEX_PAGE -o i_page.h
python3 temp/html_to_h.py temp/c_page.html -n CONFIG_PAGE -o c_page.h
python3 temp/html_to_h.py temp/t_page.html -n TASK_LIST_PAGE -o t_page.h

