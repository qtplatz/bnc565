
function submitText( text )
{
    var xmlhttp=new XMLHttpRequest();
    xmlhttp.onreadystatechange=function() {
	if (xmlhttp.readyState==4 && xmlhttp.status==200) {
	    document.getElementById("txtHint").innerHTML=xmlhttp.responseText;
	}
    }

    var json = JSON.stringify( text.split("\n") );

    console.log( json );

    xmlhttp.open("POST","/dg/ctl?submit.text=" + json, true);
    xmlhttp.send();
}
