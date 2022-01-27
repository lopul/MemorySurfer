/*
	Author: Lorenz Pullwitt <memorysurfer@lorenz-pullwitt.de>
	Copyright 2022

	This file (ms.js - v1.0.0.16) is part of MemorySurfer.

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
const msfSurrBtn = document.getElementById("msf-surround");
const msfUnformatBtn = document.getElementById("msf-unformat-btn");
const msfBrBtn = document.getElementById("msf-br");
const msfSurrDlg = document.getElementById("msf-inl-dlg");
const msfInlList = document.getElementById("msf-format-inline");
const msfInlApply = document.getElementById("msf-inl-apply");
const msfInlCancel = document.getElementById("msf-inl-cancel");
const msfDataDlg = document.getElementById("msf-data");
const msfDataBtn = document.getElementById("msf-menu");
const msfCancelDataBtn = document.getElementById("msf-data-close");
const msfQA = document.querySelectorAll(".qa-html");

document.addEventListener('DOMContentLoaded', msfOnDOMContentLoaded);

function msfOnDOMContentLoaded() {
	if (msfUnlock != null) {
		document.addEventListener('selectionchange', msfSelChanged);
		msfForm.addEventListener('submit', msfOnSubmit);
		msfUnlock.addEventListener('click', msfOnUnlock);
		msfSurrBtn.addEventListener('click', msfShowSurround);
		msfUnformatBtn.addEventListener('click', msfOnUnformat);
		msfBrBtn.addEventListener('click', msfOnBr);
		msfInlApply.addEventListener('click', msfOnApply);
		msfInlCancel.addEventListener('click', msfOnCancel);
	}
	if (msfDataBtn != null) {
		msfDataBtn.addEventListener('click', msfShowData);
		msfCancelDataBtn.addEventListener('click', msfCloseData);
	}
}

function msfSelChanged() {
	let selection;
	let type;
	let range;
	let rangeCount;
	let startContainer;
	let endContainer;
	let parentNode;
	let i;
	let isChildOfQA;
	let canFormat;
	let value;
	let selected;
	if (msfUnlock != null && msfUnlock.checked) {
		isChildOfQA = false;
		selection = window.getSelection();
		type = selection.type;
		if (type === 'Caret') {
			rangeCount = selection.rangeCount;
			if (rangeCount === 1) {
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
			}
		} else if (type === 'Range') {
			rangeCount = selection.rangeCount;
			if (rangeCount === 1) {
				range = selection.getRangeAt(0);
				startContainer = range.startContainer;
				endContainer = range.endContainer;
				canFormat = startContainer.nodeType == Node.TEXT_NODE && endContainer.nodeType == Node.TEXT_NODE && startContainer.parentNode === endContainer.parentNode;
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
	}
	msfSurrBtn.disabled = !msfUnlock.checked || type !== 'Range' || isChildOfQA !== true || canFormat !== true;
	msfUnformatBtn.disabled = !msfUnlock.checked || type !== 'Range' || isChildOfQA !== true;
	msfBrBtn.disabled = !msfUnlock.checked || type !== 'Caret' || isChildOfQA !== true;
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

function msfShowSurround() {
	msfSurrDlg.style.visibility = 'visible';
}

function msfOnUnformat() {
	document.execCommand("removeFormat", false, null);
}

function msfOnBr() {
	let checked;
	let selection;
	let type;
	let range;
	let parentNode;
	let i;
	let isChildOfQA;
	let newElem;
	checked = msfUnlock.checked;
	if (checked) {
		selection = window.getSelection();
		type = selection.type;
		if (type === 'Caret') {
			range = selection.getRangeAt(0);
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
				newElem = document.createElement('br');
				if (newElem) {
					range.insertNode(newElem);
					selection.removeAllRanges();
					range = new Range();
					range.setStartAfter(newElem);
					range.setEndAfter(newElem);
					selection.addRange(range);
				}
			}
		}
	}
}

function msfOnApply() {
	let checked;
	let selection;
	let type;
	let range;
	let rangeCount;
	let startContainer;
	let endContainer;
	let parentNode;
	let i;
	let isChildOfQA;
	let canFormat;
	let tagName;
	let newParent;
	checked = msfUnlock.checked;
	if (checked) {
		selection = window.getSelection();
		type = selection.type;
		if (type === 'Range') {
			rangeCount = selection.rangeCount;
			if (rangeCount === 1) {
				range = selection.getRangeAt(0);
				startContainer = range.startContainer;
				endContainer = range.endContainer;
				canFormat = startContainer.nodeType == Node.TEXT_NODE && endContainer.nodeType == Node.TEXT_NODE && startContainer.parentNode === endContainer.parentNode;
				if (canFormat) {
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
	msfSurrDlg.style.visibility = 'hidden';
}

function msfOnCancel() {
	msfSurrDlg.style.visibility = 'hidden';
}

function msfShowData() {
	msfDataDlg.style.visibility = 'visible';
}

function msfCloseData() {
	msfDataDlg.style.visibility = 'hidden';
}
