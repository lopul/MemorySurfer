/*
	Author: Lorenz Pullwitt <memorysurfer@lorenz-pullwitt.de>
	Copyright 2022

	This file (ms.js - v1.0.0.13) is part of MemorySurfer.

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

const msfForm = document.querySelector("form");
const msfUnlock = document.getElementById("msf-unlock");
const msfInlBtn = document.getElementById("msf-inl-btn");
const msfUnformatBtn = document.getElementById("msf-unformat-btn");
const msfInlDlg = document.getElementById("msf-inl-dlg");
const msfInlList = document.getElementById("msf-format-inline");
const msfInlApply = document.getElementById("msf-inl-apply");
const msfInlCancel = document.getElementById("msf-inl-cancel");
const msfQA = document.querySelectorAll(".qa-html");

document.addEventListener('selectionchange', msf_selChanged);
msfForm.addEventListener('submit', msfOnSubmit);
msfUnlock.addEventListener('click', msfOnUnlock);
msfInlBtn.addEventListener('click', msfOnInline);
msfUnformatBtn.addEventListener('click', msfOnUnformat);
msfInlApply.addEventListener('click', msfOnApply);
msfInlCancel.addEventListener('click', msfOnCancel);

function msf_selChanged() {
	let range;
	let selection;
	let startContainer;
	let endContainer;
	let parentNode;
	let isChildOfQA;
	let canFormat;
	let i;
	let value;
	let selected;
	if (msfUnlock.checked) {
		isChildOfQA = false;
		selection = document.getSelection();
		if (selection.type === "Caret") {
			range = selection.getRangeAt(0);
			parentNode = range.commonAncestorContainer;
			while (parentNode !== null && !isChildOfQA) {
				for (i = 0; i < msfQA.length && !isChildOfQA; i++) {
					isChildOfQA = msfQA[i] === parentNode;
				}
				if (!isChildOfQA) {
					parentNode = parentNode.parentNode;
				}
			}
		} else if (selection.type === "Range") {
			range = selection.getRangeAt(0);
			startContainer = range.startContainer;
			endContainer = range.endContainer;
			canFormat = startContainer === endContainer;
			parentNode = range.commonAncestorContainer;
			while (parentNode !== null && !isChildOfQA) {
				for (i = 0; i < msfQA.length && !isChildOfQA; i++) {
					isChildOfQA = msfQA[i] === parentNode;
				}
				if (!isChildOfQA) {
					parentNode = parentNode.parentNode;
				}
			}
		}
	}
	msfInlBtn.disabled = !msfUnlock.checked || selection.type !== "Range" || isChildOfQA !== true || canFormat !== true;
	msfUnformatBtn.disabled = !msfUnlock.checked || selection.type !== "Range" || isChildOfQA !== true;
}

function msfOnSubmit() {
	let checked;
	let q;
	let a;
	if (msfQA.length > 0) {
		checked = msfUnlock.checked;
		if (checked) {
			q = document.querySelector('input[name = "q"]');
			a = document.querySelector('input[name = "a"]');
			q.value = msfQA[0].innerHTML;
			a.value = msfQA[1].innerHTML;
		}
	}
}

function msfOnUnlock() {
	let checked;
	let isTxt;
	let i;
	isTxt = msfQA.length == 0;
	if (!isTxt) {
		checked = msfUnlock.checked;
		for (i = 0; i < msfQA.length; i++) {
			msfQA[i].contentEditable = checked;
		}
	} else {
		window.alert("A card in TXT format can't be unlocked.");
		msfUnlock.checked = false;
	}
}

function msfOnInline() {
	msfInlDlg.style.visibility = 'visible';
}

function msfOnUnformat() {
	document.execCommand("removeFormat", false, null);
}

function msfOnApply() {
	let checked;
	let selection;
	let range;
	let type;
	let rangeCount;
	let startContainer;
	let endContainer;
	let parentNode;
	let isChildOfQA;
	let i;
	let tagName;
	let newParent;
	checked = msfUnlock.checked;
	if (checked) {
		selection = window.getSelection();
		type = selection.type;
		if (type === "Range") {
			rangeCount = selection.rangeCount;
			if (rangeCount === 1) {
				range = selection.getRangeAt(0);
				startContainer = range.startContainer;
				endContainer = range.endContainer;
				if (startContainer.parentNode === endContainer.parentNode) {
					parentNode = range.commonAncestorContainer;
					isChildOfQA = false;
					while (parentNode !== null && !isChildOfQA) {
						for (i = 0; i < msfQA.length && !isChildOfQA; i++) {
							isChildOfQA = msfQA[i] === parentNode;
						}
						if (!isChildOfQA) {
							parentNode = parentNode.parentNode;
						}
					}
					if (isChildOfQA) {
						tagName = msfInlList.value;
						newParent = document.createElement(tagName);
						if (newParent !== null) {
							try {
								range.surroundContents(newParent);
							} catch (error) {
								alert(error);
							}
						}
					}
				}
			}
		}
	}
	msfInlDlg.style.visibility = 'hidden';
}

function msfOnCancel() {
	msfInlDlg.style.visibility = 'hidden';
}
