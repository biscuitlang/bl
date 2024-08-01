let bl_search = document.getElementById("bl-search");
let bl_search_results = document.getElementById("bl-search-results");

let db = []

fetch("./search.json").then(res => res.json()).then(data => {
    db = data
    update_search_results(bl_search.value)
})

function first_from_left(str, c, start) {
    if (start >= str.length) return -1
    for (let i = start; i < str.length; ++i) {
        if (str[i].toUpperCase() === c.toUpperCase()) return i
    }
    return -1
}

function fuzzy_cmp(str, other) {
    if (other.length === 0) return 0
    const MAX = 256
    let score = 0
    for (let i = 0; i < other.length; ++i) {
        let index = first_from_left(str, other[i], i)
        if (index < 0) return -1
        score += MAX - index
    }
    return score
}

function search(subject) {
    let result = []
    db.forEach(item => {
        let score = fuzzy_cmp(item.text, subject)
        if (score >= 0) {
            result.push({
                score: score,
                text: item.text,
                url: item.file + "#" + item.id
            })
        }
    });

    result.sort((a, b) => b.score - a.score)
    return result
}

function update_search_results(subject) {
    bl_search_results.innerHTML = ''
    if (subject === '') {
        return
    }
    let results = search(subject)
    results.forEach(item => {
        if (item.text.includes(subject)) {
            const li = document.createElement('li')
            const a = document.createElement('a')
            a.textContent = item.text
            a.href = item.url
            li.appendChild(a)
            bl_search_results.appendChild(li)
        }
    });
}

bl_search.addEventListener("input", (e) => {
    const subject = e.target.value
    sessionStorage.setItem('searchInput', subject);
    update_search_results(subject)
})

bl_search.addEventListener("focus", () => bl_search.select());

document.addEventListener('DOMContentLoaded', () => {
    const savedText = sessionStorage.getItem('searchInput')
    if (savedText) {
        bl_search.value = savedText
    }
})