function handleOnOff( _this ) {

    var xmlhttp=new XMLHttpRequest();
    xmlhttp.onreadystatechange=function() {
	if (xmlhttp.readyState==4 && xmlhttp.status==200) {
	    document.getElementById("txtHint").innerHTML=xmlhttp.responseText
	}
    }

    var id = _this.id;
    var value = _this.checked;
    xmlhttp.open("GET","/dg/ctl?set="+JSON.stringify( { checkbox: [{id, value}] } ), true)
    xmlhttp.send()
}

function handleClicked( _this ) {

    var xmlhttp=new XMLHttpRequest();
    xmlhttp.onreadystatechange=function() {
	if (xmlhttp.readyState==4 && xmlhttp.status==200) {
	    document.getElementById("txtHint").innerHTML=xmlhttp.responseText
	}
    }

    var id = _this.id;
    var value = _this.checked;
    xmlhttp.open("GET","/dg/ctl?set="+JSON.stringify( { clicked: [{id, value}] } ), true)
    xmlhttp.send()
}

function submitCmdText() {

    console.log( text );
}

$(function() {
    $( "[id^=switch]" ).change(function() {
	handleOnOff(this)
    });
    
    $( "[id^=fsm]" ).change(function() {
	handleOnOff(this)
    });
    
    $( "[id^=button-commit]" ).on( 'click', function( e ) {
	commitData();
    });

    $( "[id^=button-fetch]" ).on( 'click', function( e ) {
	fetchStatus();
    });

    $( "[id^=button-submit]" ).on( 'click', function( e ) {
	var text = $('textarea#cmdtext').val();
	submitText( text );
    });

});

