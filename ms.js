/*
	Author: Lorenz Pullwitt <memorysurfer@lorenz-pullwitt.de>
	Copyright 2022

	This file is part of MemorySurfer.

	MemorySurfer is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, you can find it here:
	https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
*/

"use strict";

const msf_form = document.querySelector("form");
const msf_unlock = document.querySelector("#msf-unlock");
const msf_qa = document.querySelectorAll(".qa-html");

msf_form.addEventListener('submit', msf_onSubmit);
msf_unlock.addEventListener('click', msf_toggle);

function msf_onSubmit() {
	let checked;
	let q;
	let a;
	if (msf_qa.length > 0) {
		checked = msf_unlock.checked;
		if (checked) {
			q = document.querySelector('input[name = "q"]');
			a = document.querySelector('input[name = "a"]');
			q.value = msf_qa[0].innerHTML;
			a.value = msf_qa[1].innerHTML;
		}
	}
}

function msf_toggle() {
	let checked;
	let is_txt;
	let i;
	checked = msf_unlock.checked;
	if (checked) {
		is_txt = msf_qa.length == 0;
		if (!is_txt) {
			for (i = 0; i < msf_qa.length; i++) {
				msf_qa[i].contentEditable = checked;
			}
		} else {
			window.alert("A card in TXT format can't be unlocked.");
			msf_unlock.checked = false;
		}
	}
}
